/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bmp280.h"
#include "cli.h"
#include "config.h"
#include "ds18b20.h"
#include "gps_neo6m.h"
#include "mpu9250.h"
#include "speed.h"
#include "tm1638.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define M_PI 3.14
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

I2C_HandleTypeDef hi2c1;

SD_HandleTypeDef hsd;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi2_tx;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim14;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_CAN1_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SDIO_SD_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM5_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_TIM14_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
CAN_TxHeaderTypeDef TxHeader;
CAN_RxHeaderTypeDef RxHeader;
uint8_t TxData[8];
uint8_t RxData[8];
uint32_t TxMailbox;

volatile float can_vbus = 0.0f;
volatile float can_actual_iq = 0.0f;
volatile uint8_t can_motor_active = 0;
volatile uint32_t last_can_motor_tick = 0;

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
    Error_Handler();
  }

  if (RxHeader.StdId == 0x20) {
    HAL_GPIO_TogglePin(USER_LED_GPIO_Port, USER_LED_Pin);
    can_motor_active = RxData[0];
    uint16_t vbus_centi = (uint16_t)(RxData[1] | (RxData[2] << 8));
    int16_t iq_centi = (int16_t)(RxData[3] | (RxData[4] << 8));

    can_vbus = (float)vbus_centi / 100.0f;
    can_actual_iq = (float)iq_centi / 100.0f;
    last_can_motor_tick = HAL_GetTick();
  }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  Speed_IC_Callback(htim);
}

#define MOTOR_DEFAULT_RPM 1000
#define MOTOR_RPM_STEP 100
#define MOTOR_RPM_MIN 100
#define MOTOR_RPM_MAX 5000

void CAN_SendMotorControl(uint8_t run_state, uint16_t rpm) {
  TxHeader.StdId = 0x10;
  TxHeader.ExtId = 0x00;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.DLC = 8;
  TxHeader.TransmitGlobalTime = DISABLE;

  TxData[0] = run_state;
  TxData[1] = (uint8_t)(rpm & 0xFF);
  TxData[2] = (uint8_t)((rpm >> 8) & 0xFF);
  TxData[3] = 0;
  TxData[4] = 0;
  TxData[5] = 0;
  TxData[6] = 0;
  TxData[7] = 0;

  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0) {
    HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);
  }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CAN1_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_SDIO_SD_Init();
  MX_SPI2_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  MX_USART6_UART_Init();
  MX_USB_DEVICE_Init();
  MX_FATFS_Init();
  MX_TIM14_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim14); // Start Timer 14 for microsecond delays
  CAN_FilterTypeDef canfilterconfig;

  canfilterconfig.FilterBank = 0;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canfilterconfig.FilterIdHigh = 0x0000;
  canfilterconfig.FilterIdLow = 0x0000;
  canfilterconfig.FilterMaskIdHigh = 0x0000;
  canfilterconfig.FilterMaskIdLow = 0x0000;
  canfilterconfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  canfilterconfig.FilterActivation = ENABLE;
  canfilterconfig.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_CAN_Start(&hcan1) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) !=
      HAL_OK) {
    Error_Handler();
  }

  TxHeader.StdId = 0x10;
  TxHeader.ExtId = 0x01;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 8;
  TxHeader.TransmitGlobalTime = DISABLE;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  TM1638_Init();
  TM1638_Clear();

  BMP280_Init(&hspi1);
  MPU9250_Init(&hspi1);
  GPS_Init(&huart1);

  Config_Init();
  CLI_Init();
  Speed_Init();

  uint8_t last_raw_buttons = 0;
  uint8_t debounced_buttons = 0;
  uint8_t prev_debounced_buttons = 0;
  uint32_t last_btn_action_tick[8] = {0};

  char display_str[16];

  uint16_t motor_target_rpm = MOTOR_DEFAULT_RPM;
  uint8_t stop_packets_remaining = 0;
  uint32_t btn2_hold_time = 0;
  uint32_t btn3_hold_time = 0;
  uint32_t last_btn_repeat = 0;

  uint8_t display_mode = 0; // 0=Set RPM, 1=Accel/Gyro, 2=Temp/Baro, 3=GPS, 4=Motor/Efficiency
  uint8_t sub_mode = 0;     // Sub-mode for current display_mode

  float accumulated_distance_km = 0.0f;
  float accumulated_energy_ws = 0.0f;
  float accumulated_energy_kwh = 0.0f;
  float km_per_kwh = 0.0f;
  uint32_t last_energy_tick = 0;

  BMP280_Data bmp_data = {0};
  MPU9250_Data imu_data = {0};
  GPS_Data gps_data = {0};

  DS18B20_Device ds18b20_devs[2];
  uint8_t num_ds18b20 = DS18B20_Search(GPIOB, GPIO_PIN_10, ds18b20_devs, 2);
  uint32_t last_temp_req = 0;
  uint8_t temp_req_pending = 0;

#pragma pack(push, 1)
  typedef struct {
    uint32_t timestamp_ms; // Fallback tick timestamp
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t gps_time_valid;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float altitude;
    float latitude;
    float longitude;
    float gps_altitude;
    float pdop;
    uint8_t fix_type;
    uint8_t num_satellites;
    float speed_left;
    float speed_right;
    float vbus;
    float iq;
    float rpm_right;
    float rpm_left;
    float temp1;
    float temp2;
  } LogData;
#pragma pack(pop)

  uint8_t is_logging = 0;
  uint8_t sd_error = 0;
  uint32_t log_start_time = 0;

  uint32_t last_ui_tick = 0;
  uint32_t last_log_tick = 0;
  uint32_t last_sync_tick = 0;

  char current_filename[16] = {0};

  while (1) {
    CLI_Task();

    uint32_t current_tick = HAL_GetTick();

    // --- DS18B20 Periodic Search Retry (if < 2 sensors detected) ---
    static uint32_t last_ds18b20_search = 0;
    if (num_ds18b20 < 2 && current_tick - last_ds18b20_search >= 3000) {
      last_ds18b20_search = current_tick;
      uint8_t found = DS18B20_Search(GPIOB, GPIO_PIN_10, ds18b20_devs, 2);
      if (found > num_ds18b20) {
        num_ds18b20 = found;
      }
    }

    // --- DS18B20 Async State Machine ---
    if (num_ds18b20 > 0) {
      if (!temp_req_pending && current_tick - last_temp_req >= 1000) {
        DS18B20_StartAll(GPIOB, GPIO_PIN_10);
        last_temp_req = current_tick;
        temp_req_pending = 1;
      } else if (temp_req_pending && current_tick - last_temp_req >= 800) {
        for (int i = 0; i < num_ds18b20; i++) {
          ds18b20_devs[i].temperature =
              DS18B20_ReadTemp(GPIOB, GPIO_PIN_10, ds18b20_devs[i].rom_code);
        }
        temp_req_pending = 0;
        last_temp_req = current_tick;
      }
    }

    // --- Logging Task (Configurable Interval) ---
    // Must run BEFORE UI Task to prevent SDIO FIFO polling from colliding with
    // SPI2 DMA!
    if (is_logging) {
      if (current_tick - last_log_tick >=
          (uint32_t)current_config.log_interval_ms) {
        last_log_tick = current_tick;

        // Calculate Wheel RPM & Temperature with robust fallbacks
        float log_r1 = rpm_tim2ch1;
        float log_r2 = rpm_tim5ch2;
        if (log_r1 <= 0.0f && speed_right_kmh > 0.0f && current_config.wheel_diameter_mm > 0.0f) {
          log_r1 = (speed_right_kmh * 1000000.0f) / (M_PI * current_config.wheel_diameter_mm * 60.0f);
        }
        if (log_r2 <= 0.0f && speed_left_kmh > 0.0f && current_config.wheel_diameter_mm > 0.0f) {
          log_r2 = (speed_left_kmh * 1000000.0f) / (M_PI * current_config.wheel_diameter_mm * 60.0f);
        }

        float log_t1 = 0.0f;
        float log_t2 = 0.0f;
        if (num_ds18b20 >= 2) {
          log_t1 = (ds18b20_devs[0].temperature > -50.0f && ds18b20_devs[0].temperature < 125.0f && ds18b20_devs[0].temperature != 0.0f)
                  ? ds18b20_devs[0].temperature : bmp_data.temperature;
          log_t2 = (ds18b20_devs[1].temperature > -50.0f && ds18b20_devs[1].temperature < 125.0f && ds18b20_devs[1].temperature != 0.0f)
                  ? ds18b20_devs[1].temperature : bmp_data.temperature;
        } else if (num_ds18b20 == 1) {
          log_t1 = (ds18b20_devs[0].temperature > -50.0f && ds18b20_devs[0].temperature < 125.0f && ds18b20_devs[0].temperature != 0.0f)
                  ? ds18b20_devs[0].temperature : bmp_data.temperature;
          log_t2 = bmp_data.temperature;
        } else {
          log_t1 = bmp_data.temperature;
          log_t2 = bmp_data.temperature;
        }

        LogData log_entry;
        log_entry.timestamp_ms = current_tick - log_start_time;
        log_entry.year = gps_data.year;
        log_entry.month = gps_data.month;
        log_entry.day = gps_data.day;
        log_entry.hour = gps_data.hour;
        log_entry.min = gps_data.min;
        log_entry.sec = gps_data.sec;
        log_entry.gps_time_valid = gps_data.is_time_valid;

        log_entry.accel_x = imu_data.accel_x;
        log_entry.accel_y = imu_data.accel_y;
        log_entry.accel_z = imu_data.accel_z;
        log_entry.gyro_x = imu_data.gyro_x;
        log_entry.gyro_y = imu_data.gyro_y;
        log_entry.gyro_z = imu_data.gyro_z;
        log_entry.altitude = bmp_data.altitude;
        log_entry.latitude = gps_data.latitude;
        log_entry.longitude = gps_data.longitude;
        log_entry.gps_altitude = gps_data.gps_altitude;
        log_entry.pdop = gps_data.pdop;
        log_entry.fix_type = gps_data.fix_type;
        log_entry.num_satellites = gps_data.num_satellites;

        log_entry.speed_left = speed_left_kmh;
        log_entry.speed_right = speed_right_kmh;
        log_entry.vbus = can_vbus;
        log_entry.iq = can_actual_iq;
        log_entry.rpm_right = log_r1;
        log_entry.rpm_left = log_r2;
        log_entry.temp1 = log_t1;
        log_entry.temp2 = log_t2;

        UINT bytes_written = 0;
        FRESULT w_res =
            f_write(&SDFile, &log_entry, sizeof(LogData), &bytes_written);
        if (w_res != FR_OK || bytes_written < sizeof(LogData)) {
          CLI_Print("Log err: write %d (wrote %u/%u). Recovering SD...\r\n",
                    w_res, bytes_written, (uint32_t)sizeof(LogData));
          sd_error = 1;

          // Auto Recovery Mechanism
          f_close(&SDFile);
          f_mount(NULL, SDPath, 1); // Force unmount

          if (f_mount(&SDFatFS, SDPath, 1) == FR_OK) {
            if (f_open(&SDFile, current_filename, FA_OPEN_APPEND | FA_WRITE) ==
                FR_OK) {
              // Enforce alignment to sizeof(LogData) so fptr never shifts off record boundaries
              FSIZE_t cur_sz = f_size(&SDFile);
              FSIZE_t aligned_sz = (cur_sz / sizeof(LogData)) * sizeof(LogData);
              if (cur_sz != aligned_sz) {
                f_lseek(&SDFile, aligned_sz);
              }
              last_sync_tick = HAL_GetTick();
              CLI_Print("SD Recovered (Aligned to %lu)!\r\n", (uint32_t)aligned_sz);
              sd_error = 0;
            } else {
              is_logging = 0; // Fatal error, stop logging
              CLI_Print("SD Recovery failed\r\n");
            }
          } else {
            is_logging = 0;
            CLI_Print("SD Mount failed\r\n");
          }
        }

        // Periodic sync every 2000 ms to avoid microSD flash wear latency
        if (is_logging && (current_tick - last_sync_tick >= 2000)) {
          FRESULT s_res = f_sync(&SDFile);
          if (s_res != FR_OK) {
            CLI_Print("Log err: sync %d. Recovering SD...\r\n", s_res);
            sd_error = 1;
            // Same recovery logic for sync
            f_close(&SDFile);
            f_mount(NULL, SDPath, 1);
            if (f_mount(&SDFatFS, SDPath, 1) == FR_OK) {
              if (f_open(&SDFile, current_filename,
                         FA_OPEN_APPEND | FA_WRITE) == FR_OK) {
                FSIZE_t cur_sz = f_size(&SDFile);
                FSIZE_t aligned_sz = (cur_sz / sizeof(LogData)) * sizeof(LogData);
                if (cur_sz != aligned_sz) {
                  f_lseek(&SDFile, aligned_sz);
                }
                last_sync_tick = HAL_GetTick();
                CLI_Print("SD Recovered (Aligned to %lu)!\r\n", (uint32_t)aligned_sz);
                sd_error = 0;
              } else {
                is_logging = 0;
              }
            } else {
              is_logging = 0;
            }
          }
          last_sync_tick = current_tick;
        }
      } // End if (interval)
    } // End if (is_logging)

    // --- UI and Sensor Task (20 Hz / 50ms) ---
    if (current_tick - last_ui_tick >= 50) {
      last_ui_tick = current_tick;

      // 1. Read and Debounce TM1638 Buttons
      uint8_t raw_buttons = TM1638_ReadButtons();
      // Require 2 consecutive matching 50ms samples to eliminate bounce/glitches
      if (raw_buttons == last_raw_buttons) {
        debounced_buttons = raw_buttons;
      }
      last_raw_buttons = raw_buttons;

      // Detect rising edges (0 -> 1) and falling edges (1 -> 0)
      uint8_t pressed_edges = debounced_buttons & ~prev_debounced_buttons;
      uint8_t released_edges = ~debounced_buttons & prev_debounced_buttons;

      // --- Motor Throttle & Speed Controls ---
      // Button 1 (S1 = 0x01): Throttle / Gas (Momentary)
      uint8_t throttle_active = (debounced_buttons & 0x01) ? 1 : 0;
      if (throttle_active) {
        CAN_SendMotorControl(1, motor_target_rpm);
      } else if (released_edges & 0x01) {
        // Throttle just released: return display to Set RPM and send stop packets
        display_mode = 0;
        CAN_SendMotorControl(0, 0);
        stop_packets_remaining = 3;
      } else if (stop_packets_remaining > 0) {
        CAN_SendMotorControl(0, 0);
        stop_packets_remaining--;
      }

      // Button 2 (S2 = 0x02): Speed Down (Decrement target speed)
      if ((pressed_edges & 0x02) && (current_tick - last_btn_action_tick[1] >= 200)) {
        last_btn_action_tick[1] = current_tick;
        if (motor_target_rpm >= (MOTOR_RPM_MIN + MOTOR_RPM_STEP)) {
          motor_target_rpm -= MOTOR_RPM_STEP;
        } else {
          motor_target_rpm = MOTOR_RPM_MIN;
        }
        display_mode = 0; // Switch to Set RPM display
        btn2_hold_time = current_tick;
      } else if ((debounced_buttons & 0x02) && (current_tick - btn2_hold_time > 400) &&
                 (current_tick - last_btn_repeat > 100)) {
        if (motor_target_rpm >= (MOTOR_RPM_MIN + MOTOR_RPM_STEP)) {
          motor_target_rpm -= MOTOR_RPM_STEP;
        } else {
          motor_target_rpm = MOTOR_RPM_MIN;
        }
        display_mode = 0; // Switch to Set RPM display
        last_btn_repeat = current_tick;
      }

      // Button 3 (S3 = 0x04): Speed Up (Increment target speed)
      if ((pressed_edges & 0x04) && (current_tick - last_btn_action_tick[2] >= 200)) {
        last_btn_action_tick[2] = current_tick;
        if (motor_target_rpm + MOTOR_RPM_STEP <= MOTOR_RPM_MAX) {
          motor_target_rpm += MOTOR_RPM_STEP;
        } else {
          motor_target_rpm = MOTOR_RPM_MAX;
        }
        display_mode = 0; // Switch to Set RPM display
        btn3_hold_time = current_tick;
      } else if ((debounced_buttons & 0x04) && (current_tick - btn3_hold_time > 400) &&
                 (current_tick - last_btn_repeat > 100)) {
        if (motor_target_rpm + MOTOR_RPM_STEP <= MOTOR_RPM_MAX) {
          motor_target_rpm += MOTOR_RPM_STEP;
        } else {
          motor_target_rpm = MOTOR_RPM_MAX;
        }
        display_mode = 0; // Switch to Set RPM display
        last_btn_repeat = current_tick;
      }

      // Button 8 (S8 = 0x80): Accel & Gyro
      if ((pressed_edges & 0x80) && (current_tick - last_btn_action_tick[7] >= 250)) {
        last_btn_action_tick[7] = current_tick;
        if (display_mode == 1)
          sub_mode = (sub_mode + 1) % 6;
        else {
          display_mode = 1;
          sub_mode = 0;
        }
      }

      // Button 7 (S7 = 0x40): Temperatures and Barometer
      if ((pressed_edges & 0x40) && (current_tick - last_btn_action_tick[6] >= 250)) {
        last_btn_action_tick[6] = current_tick;
        if (display_mode == 2)
          sub_mode = (sub_mode + 1) % (2 + num_ds18b20);
        else {
          display_mode = 2;
          sub_mode = 0;
        }
      }

      // Button 6 (S6 = 0x20): GPS
      if ((pressed_edges & 0x20) && (current_tick - last_btn_action_tick[5] >= 250)) {
        last_btn_action_tick[5] = current_tick;
        if (display_mode == 3)
          sub_mode = (sub_mode + 1) % 6; // Lat, Lon, Sats, Fix, PDOP, Alt
        else {
          display_mode = 3;
          sub_mode = 0;
        }
      }

      // Button 5 (S5 = 0x10): Logging Start / Stop (400ms lockout prevents chatter restart)
      if ((pressed_edges & 0x10) && (current_tick - last_btn_action_tick[4] >= 400)) {
        last_btn_action_tick[4] = current_tick;
        if (is_logging || sd_error) {
          f_close(&SDFile);
          f_mount(NULL, SDPath, 1);
          is_logging = 0;
          sd_error = 0;
          CLI_Print("Log stopped / Err cleared\r\n");
        } else {
          FRESULT mount_res = f_mount(&SDFatFS, SDPath, 1);
          if (mount_res == FR_OK) {
            uint16_t file_index = 1;
            while (file_index < 1000) {
              sprintf(current_filename, "log%u.bin", file_index);
              FRESULT open_res =
                  f_open(&SDFile, current_filename, FA_CREATE_NEW | FA_WRITE);
              if (open_res == FR_OK) {
                is_logging = 1;
                sd_error = 0;
                log_start_time = HAL_GetTick();
                last_log_tick = log_start_time; // Reset log tick
                last_sync_tick = log_start_time; // Reset sync tick to prevent premature sync

                // Reset distance and energy baseline at start line
                accumulated_distance_km = 0.0f;
                accumulated_energy_ws = 0.0f;
                accumulated_energy_kwh = 0.0f;
                km_per_kwh = 0.0f;
                last_energy_tick = log_start_time;

                CLI_Print("Log started: %s (Baseline Reset)\r\n", current_filename);
                break;
              }
              file_index++;
            }
            if (file_index >= 1000) {
              CLI_Print("Log err: index full\r\n");
              sd_error = 1;
            }
          } else {
            CLI_Print("Log err: mount %d\r\n", mount_res);
            sd_error = 1;
          }
        }
      }

      // Button 4 (S4 = 0x08): Motor Telemetry & Efficiency (Vbus, Iq, Watt, RPM1, RPM2, km/kWh)
      if ((pressed_edges & 0x08) && (current_tick - last_btn_action_tick[3] >= 250)) {
        last_btn_action_tick[3] = current_tick;
        if (display_mode == 4)
          sub_mode = (sub_mode + 1) % 6;
        else {
          display_mode = 4;
          sub_mode = 0;
        }
      }

      prev_debounced_buttons = debounced_buttons;

      // 2. Read Sensors
      MPU9250_ReadSensor(&hspi1, &imu_data);
      BMP280_ReadSensor(&hspi1, &bmp_data);
      GPS_GetLatestData(&gps_data); // Gets latest parsed data

      // 3. Distance and Energy Accumulation (since start line / Button 5 logging)
      if (last_energy_tick == 0) {
        last_energy_tick = current_tick;
      }
      float dt_energy = (current_tick - last_energy_tick) / 1000.0f;
      last_energy_tick = current_tick;

      if (is_logging && dt_energy > 0.0f && dt_energy < 1.0f) {
        // Distance: integrated from average wheel speed
        float avg_wheel_spd = (speed_left_kmh + speed_right_kmh) / 2.0f;
        if (avg_wheel_spd > 0.0f) {
          accumulated_distance_km += (avg_wheel_spd / 3600.0f) * dt_energy;
        }

        // Electrical Energy: Power (Watt) = Vbus * Iq
        float motor_power = can_vbus * can_actual_iq;
        if (motor_power > 0.0f) {
          accumulated_energy_ws += motor_power * dt_energy;
          accumulated_energy_kwh = accumulated_energy_ws / 3600000.0f;
        }

        // Efficiency: km / kWh
        if (accumulated_energy_kwh > 0.00005f) {
          km_per_kwh = accumulated_distance_km / accumulated_energy_kwh;
        } else {
          km_per_kwh = 0.0f;
        }
      }

      // 4. Display Logic
      uint8_t led_mask = 0;

      Speed_UpdateTimeout();
      // Coast strategy warning
      float avg_speed = (speed_left_kmh + speed_right_kmh) / 2.0f;
      if (avg_speed > 0 && avg_speed < current_config.coast_speed_min) {
        if ((HAL_GetTick() / 250) % 2) {
          led_mask |= 0x40; // Blink LED 7
        }
      }

      if (throttle_active) {
        // Throttle active: Leftmost segment is 'A' (indicator), followed by
        // target speed
        snprintf(display_str, sizeof(display_str), "A%7u", motor_target_rpm);
        led_mask |= 0x01; // LED 1 indicates throttle command is being sent
      } else {
        switch (display_mode) {
        case 0: // Set RPM display
          snprintf(display_str, sizeof(display_str), "SET%5u",
                   motor_target_rpm);
          led_mask |= 0x02; // LED 2 indicates set RPM display
          break;

        case 1: // Accel & Gyro
          led_mask = 1 << sub_mode;
          if (sub_mode == 0)
            sprintf(display_str, "Ax %5.2f", imu_data.accel_x);
          else if (sub_mode == 1)
            sprintf(display_str, "Ay %5.2f", imu_data.accel_y);
          else if (sub_mode == 2)
            sprintf(display_str, "Az %5.2f", imu_data.accel_z);
          else if (sub_mode == 3)
            sprintf(display_str, "gx %5.1f", imu_data.gyro_x);
          else if (sub_mode == 4)
            sprintf(display_str, "gy %5.1f", imu_data.gyro_y);
          else if (sub_mode == 5)
            sprintf(display_str, "gz %5.1f", imu_data.gyro_z);
          break;

        case 2: // Temperatures dan baro
          led_mask = 1 << sub_mode;
          if (sub_mode == 0)
            sprintf(display_str, "b %6.2f", bmp_data.temperature);
          else if (sub_mode == 1) // Show altitude with 3 decimal places
            sprintf(display_str, "%8.3f", bmp_data.altitude);
          else if (sub_mode == 2) {
            if (num_ds18b20 > 0)
              sprintf(display_str, "d1%6.2f", ds18b20_devs[0].temperature);
            else
              sprintf(display_str, "d1 NONE ");
          } else if (sub_mode == 3) {
            if (num_ds18b20 > 1)
              sprintf(display_str, "d2%6.2f", ds18b20_devs[1].temperature);
            else
              sprintf(display_str, "d2 NONE ");
          }
          break;
        case 3:                     // GPS
          led_mask = 1 << sub_mode; // LED 1 to 6
          if (gps_data.is_valid || sub_mode >= 2) {
            if (sub_mode == 0)
              sprintf(display_str, "%8.4f", gps_data.latitude);
            else if (sub_mode == 1)
              sprintf(display_str, "%8.4f", gps_data.longitude);
            else if (sub_mode == 2)
              sprintf(display_str, "SATS  %2d", gps_data.num_satellites);
            else if (sub_mode == 3)
              sprintf(display_str, "FIX    %d", gps_data.fix_type);
            else if (sub_mode == 4)
              sprintf(display_str, "DOP %4.1f", gps_data.pdop);
            else if (sub_mode == 5)
              sprintf(display_str, "ALT%5.1f", gps_data.gps_altitude);
          } else {
            sprintf(display_str, "NO  GPS ");
          }
          break;

        case 4: // Motor Telemetry & Efficiency (Button 4)
          led_mask = 1 << sub_mode; // LED 1 to 6
          if (sub_mode == 0) {
            // DC Bus Voltage (V)
            sprintf(display_str, "U  %5.2f", can_vbus);
          } else if (sub_mode == 1) {
            // Actual Iq (A)
            sprintf(display_str, "A  %5.2f", can_actual_iq);
          } else if (sub_mode == 2) {
            // Motor Power (Watt = Vbus * Iq)
            float power_w = can_vbus * can_actual_iq;
            sprintf(display_str, "P  %5.1f", power_w);
          } else if (sub_mode == 3) {
            // RPM Timer 2 CH1 (Right Wheel)
            sprintf(display_str, "r1 %5.0f", rpm_tim2ch1);
          } else if (sub_mode == 4) {
            // RPM Timer 5 CH2 (Left Wheel)
            sprintf(display_str, "r2 %5.0f", rpm_tim5ch2);
          } else if (sub_mode == 5) {
            // Efficiency (km / kWh)
            sprintf(display_str, "E  %5.1f", km_per_kwh);
          }
          break;
        }
      }

      if (sd_error) {
        led_mask |= 0x80; // Solid LED 8 on error
      } else if (is_logging) {
        if ((current_tick / 500) % 2) {
          led_mask |= 0x80; // Blink LED 8 on logging
        }
      }

      TM1638_SendDMA(display_str, led_mask);
    } // End UI Task

    // --- Coprocessor Telemetry Task (2 Hz / 500ms) ---
    static uint32_t last_telemetry_tick = 0;
    if (current_tick - last_telemetry_tick >= 500) {
      last_telemetry_tick = current_tick;

      // Temperature fallback: prioritize DS18B20; fallback to BMP280 temperature if DS18B20 is unavailable or invalid
      float temp1 = 0.0f;
      float temp2 = 0.0f;
      if (num_ds18b20 >= 2) {
        temp1 = (ds18b20_devs[0].temperature > -50.0f && ds18b20_devs[0].temperature < 125.0f && ds18b20_devs[0].temperature != 0.0f)
                ? ds18b20_devs[0].temperature : bmp_data.temperature;
        temp2 = (ds18b20_devs[1].temperature > -50.0f && ds18b20_devs[1].temperature < 125.0f && ds18b20_devs[1].temperature != 0.0f)
                ? ds18b20_devs[1].temperature : bmp_data.temperature;
      } else if (num_ds18b20 == 1) {
        temp1 = (ds18b20_devs[0].temperature > -50.0f && ds18b20_devs[0].temperature < 125.0f && ds18b20_devs[0].temperature != 0.0f)
                ? ds18b20_devs[0].temperature : bmp_data.temperature;
        temp2 = bmp_data.temperature;
      } else {
        temp1 = bmp_data.temperature;
        temp2 = bmp_data.temperature;
      }

      // Wheel RPM fallback: TIM2 is Right Wheel, TIM5 is Left Wheel.
      // If RPM is 0 while wheel speed is non-zero, calculate RPM directly from speed to prevent zero RPM in telemetry
      float r1 = rpm_tim2ch1;
      float r2 = rpm_tim5ch2;
      if (r1 <= 0.0f && speed_right_kmh > 0.0f && current_config.wheel_diameter_mm > 0.0f) {
        r1 = (speed_right_kmh * 1000000.0f) / (M_PI * current_config.wheel_diameter_mm * 60.0f);
      }
      if (r2 <= 0.0f && speed_left_kmh > 0.0f && current_config.wheel_diameter_mm > 0.0f) {
        r2 = (speed_left_kmh * 1000000.0f) / (M_PI * current_config.wheel_diameter_mm * 60.0f);
      }

      static char tele_buf[512];
      int len = snprintf(
          tele_buf, sizeof(tele_buf),
          "{\"ts\":%lu,\"y\":%u,\"m\":%u,\"d\":%u,\"h\":%u,\"min\":%u,\"s\":%u,"
          "\"v\":%u,"
          "\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
          "\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f,"
          "\"alt\":%.1f,"
          "\"lat\":%.6f,\"lon\":%.6f,\"galt\":%.1f,"
          "\"pd\":%.1f,\"fix\":%u,\"ns\":%u,"
          "\"sl\":%.1f,\"sr\":%.1f,"
          "\"vbus\":%.2f,\"iq\":%.2f,"
          "\"r1\":%.0f,\"r2\":%.0f,"
          "\"t1\":%.2f,\"t2\":%.2f}\n",
          (unsigned long)current_tick,
          (unsigned int)gps_data.year,
          (unsigned int)gps_data.month,
          (unsigned int)gps_data.day,
          (unsigned int)gps_data.hour,
          (unsigned int)gps_data.min,
          (unsigned int)gps_data.sec,
          (unsigned int)gps_data.is_time_valid,
          (double)imu_data.accel_x,
          (double)imu_data.accel_y,
          (double)imu_data.accel_z,
          (double)imu_data.gyro_x,
          (double)imu_data.gyro_y,
          (double)imu_data.gyro_z,
          (double)bmp_data.altitude,
          (double)gps_data.latitude,
          (double)gps_data.longitude,
          (double)gps_data.gps_altitude,
          (double)gps_data.pdop,
          (unsigned int)gps_data.fix_type,
          (unsigned int)gps_data.num_satellites,
          (double)speed_left_kmh,
          (double)speed_right_kmh,
          (double)can_vbus,
          (double)can_actual_iq,
          (double)r1,
          (double)r2,
          (double)temp1,
          (double)temp2);

      if (len > 0 && len < sizeof(tele_buf)) {
        // Use IT (Interrupt) to prevent blocking the main loop
        // If the previous transmission hasn't finished, this returns HAL_BUSY
        // and skips
        HAL_UART_Transmit_IT(&huart2, (uint8_t *)tele_buf, len);
      }
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief Peripherals Common Clock Configuration
 * @retval None
 */
void PeriphCommonClock_Config(void) {
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
   */
  PeriphClkInitStruct.PeriphClockSelection =
      RCC_PERIPHCLK_SDIO | RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.PLLSAI.PLLSAIM = 4;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 96;
  PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
  PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV4;
  PeriphClkInitStruct.PLLSAIDivQ = 1;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48CLKSOURCE_PLLSAIP;
  PeriphClkInitStruct.SdioClockSelection = RCC_SDIOCLKSOURCE_CLK48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief CAN1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_CAN1_Init(void) {

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 6;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 6;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE END CAN1_Init 2 */
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief SDIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_SDIO_SD_Init(void) {

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 10;
  /* USER CODE BEGIN SDIO_Init 2 */

  /* USER CODE END SDIO_Init 2 */
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief SPI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI2_Init(void) {

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_1LINE;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_LSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
}

/**
 * @brief TIM5 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM5_Init(void) {

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 4294967295;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim5) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim5, &sConfigIC, TIM_CHANNEL_2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */
}

/**
 * @brief TIM14 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM14_Init(void) {

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */

  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 0;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 65535;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM14_Init 2 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 83;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 65535;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE END TIM14_Init 2 */
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 230400;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
 * @brief USART6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART6_UART_Init(void) {

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, SPI1_CS_BARO_Pin | SPI1_CS_IMU_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, USER_LED_Pin | SPI2_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : USER_BTN_Pin */
  GPIO_InitStruct.Pin = USER_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(USER_BTN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI1_CS_BARO_Pin SPI1_CS_IMU_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_BARO_Pin | SPI1_CS_IMU_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : USER_LED_Pin SPI2_CS_Pin */
  GPIO_InitStruct.Pin = USER_LED_Pin | SPI2_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : DS18B20_Pin */
  GPIO_InitStruct.Pin = DS18B20_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DS18B20_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SDIO_DET_Pin */
  GPIO_InitStruct.Pin = SDIO_DET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SDIO_DET_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    GPS_UART_RxCpltCallback(huart);
  }
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state
   */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
     file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
