#include "gps_neo6m.h"
#include <string.h>

static UART_HandleTypeDef *gps_huart;
static uint8_t rx_data;

static GPS_Data current_gps_data = {0};
static uint8_t new_data_available = 0;

// UBX Parser State Machine
typedef enum {
  UBX_SYNC1,
  UBX_SYNC2,
  UBX_CLASS,
  UBX_ID,
  UBX_LEN_L,
  UBX_LEN_H,
  UBX_PAYLOAD,
  UBX_CK_A,
  UBX_CK_B
} UBX_State;

static UBX_State ubx_state = UBX_SYNC1;
static uint8_t ubx_class = 0;
static uint8_t ubx_id = 0;
static uint16_t ubx_len = 0;
static uint16_t ubx_payload_idx = 0;
static uint8_t ubx_ck_a = 0;
static uint8_t ubx_ck_b = 0;
static uint8_t ubx_rx_ck_a = 0;
static uint8_t
    ubx_payload[100]; // Max payload we care about is 92 bytes (NAV-PVT)

static void UBX_ProcessPayload() {
  if (ubx_class == 0x01) {                 // NAV
    if (ubx_id == 0x02 && ubx_len >= 28) { // NAV-POSLLH
      int32_t lon_raw = (ubx_payload[7] << 24) | (ubx_payload[6] << 16) |
                        (ubx_payload[5] << 8) | ubx_payload[4];
      int32_t lat_raw = (ubx_payload[11] << 24) | (ubx_payload[10] << 16) |
                        (ubx_payload[9] << 8) | ubx_payload[8];
      int32_t hmsl_raw = (ubx_payload[19] << 24) | (ubx_payload[18] << 16) |
                         (ubx_payload[17] << 8) | ubx_payload[16];

      current_gps_data.longitude = lon_raw / 10000000.0f;
      current_gps_data.latitude = lat_raw / 10000000.0f;
      current_gps_data.gps_altitude = hmsl_raw / 1000.0f;
      current_gps_data.is_valid = 1;
      new_data_available = 1;
    } else if (ubx_id == 0x06 && ubx_len >= 52) { // NAV-SOL
      current_gps_data.fix_type = ubx_payload[10];
      current_gps_data.num_satellites = ubx_payload[47];
      uint16_t pdop_raw = (ubx_payload[45] << 8) | ubx_payload[44];
      current_gps_data.pdop = pdop_raw / 100.0f;
      new_data_available = 1;
    } else if (ubx_id == 0x21 && ubx_len >= 20) { // NAV-TIMEUTC (For NEO-6M)
      uint8_t valid = ubx_payload[19];

      current_gps_data.year = (ubx_payload[13] << 8) | ubx_payload[12];
      current_gps_data.month = ubx_payload[14];
      current_gps_data.day = ubx_payload[15];
      current_gps_data.hour = ubx_payload[16];
      current_gps_data.min = ubx_payload[17];
      current_gps_data.sec = ubx_payload[18];

      // validUTC is bit 2 (0x04)
      current_gps_data.is_time_valid = (valid & 0x04) ? 1 : 0;
      new_data_available = 1;
    } else if (ubx_id == 0x07 && ubx_len >= 92) { // NAV-PVT
      // mulai diperkenalkan pada u-blox generasi 7 (seperti NEO-7M) dan
      // diwajibkan menjadi standar utama pada u-blox generasi 8 (NEO-M8N)
      // hingga generasi terbaru
      int32_t lon_raw = (ubx_payload[27] << 24) | (ubx_payload[26] << 16) |
                        (ubx_payload[25] << 8) | ubx_payload[24];
      int32_t lat_raw = (ubx_payload[31] << 24) | (ubx_payload[30] << 16) |
                        (ubx_payload[29] << 8) | ubx_payload[28];
      int32_t hmsl_raw = (ubx_payload[39] << 24) | (ubx_payload[38] << 16) |
                         (ubx_payload[37] << 8) | ubx_payload[36];
      uint16_t pdop_raw = (ubx_payload[77] << 8) | ubx_payload[76];

      uint8_t flags = ubx_payload[21];
      uint8_t valid = ubx_payload[11];

      current_gps_data.longitude = lon_raw / 10000000.0f;
      current_gps_data.latitude = lat_raw / 10000000.0f;
      current_gps_data.gps_altitude = hmsl_raw / 1000.0f;
      current_gps_data.year = (ubx_payload[5] << 8) | ubx_payload[4];
      current_gps_data.month = ubx_payload[6];
      current_gps_data.day = ubx_payload[7];
      current_gps_data.hour = ubx_payload[8];
      current_gps_data.min = ubx_payload[9];
      current_gps_data.sec = ubx_payload[10];

      current_gps_data.fix_type = ubx_payload[20];
      current_gps_data.num_satellites = ubx_payload[23];
      current_gps_data.pdop = pdop_raw / 100.0f;

      current_gps_data.is_valid = (flags & 0x01) ? 1 : 0; // gnssFixOK
      current_gps_data.is_time_valid =
          ((valid & 0x03) == 0x03) ? 1 : 0; // validDate and validTime
      new_data_available = 1;
    }
  } else if (ubx_class == 0x0A) {          // MON
    if (ubx_id == 0x04 && ubx_len >= 40) { // MON-VER
      strncpy(current_gps_data.sw_version, (char *)ubx_payload, 30);
      current_gps_data.sw_version[30] = '\0'; // Force null-termination
      strncpy(current_gps_data.hw_version, (char *)(ubx_payload + 30), 10);
      current_gps_data.hw_version[10] = '\0'; // Force null-termination
      new_data_available = 1;
    }
  }
}

static void GPS_Enable_Message(uint8_t class_id, uint8_t msg_id, uint8_t rate) {
  uint8_t cfg_msg[16] = {
      0xB5,     0x62, // Header
      0x06,     0x01, // Class CFG, ID MSG
      0x08,     0x00, // Length 8
      class_id, msg_id, 0x00, rate,
      0x00,     0x00,   0x00, 0x00, // Payload (UART1 rate)
      0x00,     0x00                // Checksum
  };
  uint8_t ck_a = 0, ck_b = 0;
  for (int i = 2; i < 14; i++) {
    ck_a = ck_a + cfg_msg[i];
    ck_b = ck_b + ck_a;
  }
  cfg_msg[14] = ck_a;
  cfg_msg[15] = ck_b;
  HAL_UART_Transmit(gps_huart, cfg_msg, sizeof(cfg_msg), 100);
}

void GPS_Init(UART_HandleTypeDef *huart) {
  gps_huart = huart;

  // 1. Explicitly enable required UBX messages (Rate = 1 per epoch)
  GPS_Enable_Message(0x01, 0x21, 1); // NAV-TIMEUTC (Time & Date)
  GPS_Enable_Message(0x01, 0x02, 1); // NAV-POSLLH (Coordinates & Altitude)
  GPS_Enable_Message(0x01, 0x06, 1); // NAV-SOL (Fix Quality, PDOP, Satellites)

  // 2. Set Power Management to Continuous / Maximum Performance (UBX-CFG-RXM)
  // lpMode = 0 (Continuous mode, no power saving)
  uint8_t cfg_rxm[] = {0xB5, 0x62, 0x06, 0x11, 0x02,
                       0x00, 0x08, 0x00, 0x21, 0x91};
  HAL_UART_Transmit(gps_huart, cfg_rxm, sizeof(cfg_rxm), 100);

  // 3. Set Dynamic Platform Model to Automotive (UBX-CFG-NAV5)
  // This improves tracking reliability at higher speeds and accelerations
  // compared to default Portable mode.
  uint8_t cfg_nav5[44] = {0xB5, 0x62, 0x06,
                          0x24, 0x24, 0x00}; // 36 bytes payload
  memset(&cfg_nav5[6], 0, 36);
  cfg_nav5[6] = 0x01; // Mask: apply dynModel only
  cfg_nav5[8] = 0x04; // dynModel: 4 = Automotive
  uint8_t nav5_ck_a = 0, nav5_ck_b = 0;
  for (int i = 2; i < 42; i++) {
    nav5_ck_a += cfg_nav5[i];
    nav5_ck_b += nav5_ck_a;
  }
  cfg_nav5[42] = nav5_ck_a;
  cfg_nav5[43] = nav5_ck_b;
  HAL_UART_Transmit(gps_huart, cfg_nav5, sizeof(cfg_nav5), 100);

  // 4. Poll MON-VER for firmware version
  uint8_t mon_ver_poll[] = {0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34};
  HAL_UART_Transmit(gps_huart, mon_ver_poll, sizeof(mon_ver_poll), 100);

  HAL_UART_Receive_IT(gps_huart, &rx_data, 1);
}

void GPS_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == gps_huart->Instance) {

    switch (ubx_state) {
    case UBX_SYNC1:
      if (rx_data == 0xB5)
        ubx_state = UBX_SYNC2;
      break;
    case UBX_SYNC2:
      if (rx_data == 0x62) {
        ubx_state = UBX_CLASS;
        ubx_ck_a = 0;
        ubx_ck_b = 0;
      } else
        ubx_state = UBX_SYNC1;
      break;
    case UBX_CLASS:
      ubx_class = rx_data;
      ubx_ck_a += rx_data;
      ubx_ck_b += ubx_ck_a;
      ubx_state = UBX_ID;
      break;
    case UBX_ID:
      ubx_id = rx_data;
      ubx_ck_a += rx_data;
      ubx_ck_b += ubx_ck_a;
      ubx_state = UBX_LEN_L;
      break;
    case UBX_LEN_L:
      ubx_len = rx_data;
      ubx_ck_a += rx_data;
      ubx_ck_b += ubx_ck_a;
      ubx_state = UBX_LEN_H;
      break;
    case UBX_LEN_H:
      ubx_len |= (rx_data << 8);
      ubx_ck_a += rx_data;
      ubx_ck_b += ubx_ck_a;
      if (ubx_len > sizeof(ubx_payload)) {
        ubx_state = UBX_SYNC1; // Payload too large, skip
      } else if (ubx_len == 0) {
        ubx_state = UBX_CK_A;
      } else {
        ubx_payload_idx = 0;
        ubx_state = UBX_PAYLOAD;
      }
      break;
    case UBX_PAYLOAD:
      ubx_payload[ubx_payload_idx++] = rx_data;
      ubx_ck_a += rx_data;
      ubx_ck_b += ubx_ck_a;
      if (ubx_payload_idx >= ubx_len) {
        ubx_state = UBX_CK_A;
      }
      break;
    case UBX_CK_A:
      ubx_rx_ck_a = rx_data;
      ubx_state = UBX_CK_B;
      break;
    case UBX_CK_B:
      if (ubx_ck_a == ubx_rx_ck_a && ubx_ck_b == rx_data) {
        UBX_ProcessPayload();
      }
      ubx_state = UBX_SYNC1;
      break;
    default:
      ubx_state = UBX_SYNC1;
      break;
    }

    // Re-enable interrupt
    HAL_UART_Receive_IT(gps_huart, &rx_data, 1);
  }
}

uint8_t GPS_GetLatestData(GPS_Data *data) {
  *data = current_gps_data;
  if (new_data_available) {
    new_data_available = 0;
    return 1;
  }
  return 0;
}
