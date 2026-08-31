#include "config.h"
#include "six_step.h"
#include "main.h"
#include "usbd_cdc_if.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

motor_config_t motor_config;

extern TIM_HandleTypeDef htim1;

// Helper to transmit string over CDC
void cdc_printf(const char *format, ...) {
  static char tx_buffer[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(tx_buffer, sizeof(tx_buffer), format, args);
  va_end(args);

  if (len > 0) {
    // Wait until TX is ready or timeout
    uint32_t timeout = 0xFFFF;
    while (CDC_Transmit_FS((uint8_t *)tx_buffer, len) == USBD_BUSY &&
           timeout > 0) {
      timeout--;
    }
  }
}

// High-speed non-blocking binary telemetry for GUI
// Packet structure: [0xAA, 0xBB] [pos:4] [vel:4] [vq:4] [target:4] [ia:4] [ib:4] [ic:4] [mode:1]
// [CRC:1] [0x55] (33 bytes total)
void Telemetry_SendBinary(float pos, float vel, float vq, float target, float ia, float ib, float ic,
                          uint8_t mode) {
  uint8_t tx_buffer[33];

  tx_buffer[0] = 0xAA;
  tx_buffer[1] = 0xBB;

  // Copy floats into buffer
  memcpy(&tx_buffer[2], &pos, 4);
  memcpy(&tx_buffer[6], &vel, 4);
  memcpy(&tx_buffer[10], &vq, 4);
  memcpy(&tx_buffer[14], &target, 4);
  memcpy(&tx_buffer[18], &ia, 4);
  memcpy(&tx_buffer[22], &ib, 4);
  memcpy(&tx_buffer[26], &ic, 4);
  tx_buffer[30] = mode;

  // Calculate simple XOR CRC for the payload
  uint8_t crc = 0;
  for (int i = 2; i < 31; i++) {
    crc ^= tx_buffer[i];
  }
  tx_buffer[31] = crc;
  tx_buffer[32] = 0x55; // Footer

  CDC_Transmit_FS(tx_buffer, 33);
}

void Config_LoadDefaults(void) {
  motor_config.magic_word = CONFIG_MAGIC_WORD;
  motor_config.pwm_freq = 20000;
  motor_config.pole_pairs = 7;     // Typical for small BLDC
  motor_config.dc_bus_voltage = 24.0f;
  motor_config.max_output_voltage = 24.0f;

  motor_config.verbose_output = 1;       // Enable GUI telemetry by default
  motor_config.verbose_period = 10;      // 10ms (100Hz) high-speed telemetry
  motor_config.open_loop_voltage = 2.0f; // 2.0V safe voltage
  motor_config.accel_limit = 100.0f;
  motor_config.switchover_rpm = 50.0f;
  motor_config.switchover_delay = 8.0f;
  motor_config.hall_offset_deg = 90.0f;
  motor_config.vel_kp = 0.05f;
  motor_config.vel_ki = 0.1f;
  motor_config.accel_rpm_s = 500.0f;

  memset(motor_config.reserved, 0, sizeof(motor_config.reserved));
}

void Config_Init(void) {
  motor_config_t *flash_config = (motor_config_t *)CONFIG_FLASH_ADDRESS;

  if (flash_config->magic_word == CONFIG_MAGIC_WORD) {
    // Load from flash
    memcpy(&motor_config, flash_config, sizeof(motor_config_t));
  } else {
    // Load defaults
    Config_LoadDefaults();
  }
}

void Config_Save(void) {
  HAL_FLASH_Unlock();

  // Erase page 63
  FLASH_EraseInitTypeDef eraseInitStruct;
  uint32_t pageError = 0;

  eraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  eraseInitStruct.Banks = FLASH_BANK_1;
  eraseInitStruct.Page = 63;
  eraseInitStruct.NbPages = 1;

  if (HAL_FLASHEx_Erase(&eraseInitStruct, &pageError) != HAL_OK) {
    cdc_printf("Error: Flash Erase Failed!\r\n");
    HAL_FLASH_Lock();
    return;
  }

  // Write structure to flash
  uint32_t *pConfig = (uint32_t *)&motor_config;
  uint32_t address = CONFIG_FLASH_ADDRESS;
  uint32_t numWords = sizeof(motor_config_t) / sizeof(uint32_t);

  for (uint32_t i = 0; i < numWords; i++) {
    if (i % 2 == 0) {
      uint64_t data = ((uint64_t)pConfig[i + 1] << 32) | pConfig[i];
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + (i * 4),
                            data) != HAL_OK) {
        cdc_printf("Error: Flash Write Failed at Word %lu\r\n", i);
        HAL_FLASH_Lock();
        return;
      }
    }
  }

  HAL_FLASH_Lock();
  cdc_printf("Config saved to flash.\r\n");
}

void Config_ApplyPWMFrequency(void) {
  // Timer is Center Aligned (if UP/DOWN counter), wait target is UP counter?
  // Let's check MX_TIM1_Init: CounterMode = TIM_COUNTERMODE_UP.
  // Period = (SystemCoreClock / pwm_freq) - 1 for Edge Aligned PWM
  uint32_t period = (HAL_RCC_GetHCLKFreq() / motor_config.pwm_freq) - 1;
  htim1.Instance->ARR = period;
}

void Config_PrintAll(void) {
  int bus_v_int = (int)motor_config.dc_bus_voltage;
  int bus_v_frac = (int)(motor_config.dc_bus_voltage * 100.0f) % 100;
  if (bus_v_frac < 0) bus_v_frac = -bus_v_frac;

  int max_v_int = (int)motor_config.max_output_voltage;
  int max_v_frac = (int)(motor_config.max_output_voltage * 100.0f) % 100;
  if (max_v_frac < 0) max_v_frac = -max_v_frac;

  int olv_i = (int)motor_config.open_loop_voltage,
      olv_f = (int)(motor_config.open_loop_voltage * 100) % 100;
  if (olv_f < 0) olv_f = -olv_f;
  
  int acc_i = (int)motor_config.accel_limit,
      acc_f = (int)(motor_config.accel_limit * 100) % 100;
  if (acc_f < 0) acc_f = -acc_f;

  cdc_printf("\r\n--- GRBL Config ---\r\n");
  cdc_printf("$0=%lu (PWM Freq Hz)\r\n", motor_config.pwm_freq);
  cdc_printf("$1=%lu (Pole Pairs)\r\n", motor_config.pole_pairs);
  cdc_printf("$2=%d.%02d (DC Bus Voltage)\r\n", bus_v_int, bus_v_frac);
  cdc_printf("$3=%d.%02d (Max Output Voltage)\r\n", max_v_int, max_v_frac);
  cdc_printf("$4=%lu (Verbose: 0=Off, 1=On)\r\n", motor_config.verbose_output);
  cdc_printf("$5=%lu (Verbose Period ms)\r\n", motor_config.verbose_period);
  cdc_printf("$6=%d.%02d (Open Loop Voltage V)\r\n", olv_i, olv_f);
  cdc_printf("$7=%d.%02d (Accel Limit rad/s^2)\r\n", acc_i, acc_f);
  
  int srm_i = (int)motor_config.switchover_rpm,
      srm_f = (int)(motor_config.switchover_rpm * 100) % 100;
  if (srm_f < 0) srm_f = -srm_f;
  cdc_printf("$8=%d.%02d (Switchover RPM)\r\n", srm_i, srm_f);
  
  int sdelay_i = (int)motor_config.switchover_delay,
      sdelay_f = (int)(motor_config.switchover_delay * 100) % 100;
  if (sdelay_f < 0) sdelay_f = -sdelay_f;
  cdc_printf("$9=%d.%02d (Switchover Delay sec)\r\n", sdelay_i, sdelay_f);
  
  int hoff_i = (int)motor_config.hall_offset_deg,
      hoff_f = (int)(motor_config.hall_offset_deg * 100) % 100;
  if (hoff_f < 0) hoff_f = -hoff_f;
  cdc_printf("$10=%d.%02d (Hall Offset Deg)\r\n", hoff_i, hoff_f);

  int vkp_i = (int)motor_config.vel_kp,
      vkp_f = (int)(motor_config.vel_kp * 1000) % 1000;
  if (vkp_f < 0) vkp_f = -vkp_f;
  cdc_printf("$11=%d.%03d (Velocity Kp)\r\n", vkp_i, vkp_f);
  
  int vki_i = (int)motor_config.vel_ki,
      vki_f = (int)(motor_config.vel_ki * 1000) % 1000;
  if (vki_f < 0) vki_f = -vki_f;
  cdc_printf("$12=%d.%03d (Velocity Ki)\r\n", vki_i, vki_f);
  
  int arpms_i = (int)motor_config.accel_rpm_s,
      arpms_f = (int)(motor_config.accel_rpm_s * 10) % 10;
  if (arpms_f < 0) arpms_f = -arpms_f;
  cdc_printf("$13=%d.%d (Accel RPM/s)\r\n", arpms_i, arpms_f);

  cdc_printf("ok\r\n");
}

void Config_PrintHelp(void) {
  cdc_printf("\r\n--- Commands ---\r\n");
  cdc_printf("$$      : Show all settings\r\n");
  cdc_printf("$x=y    : Set parameter x to value y\r\n");
  cdc_printf("$save   : Save settings to flash\r\n");
  cdc_printf("$h      : Show this help\r\n");
  cdc_printf("S<val>  : Set Target Duty Cycle (%)\r\n");
  cdc_printf("T       : Stop Motor\r\n");
  cdc_printf("ok\r\n");
}

void Config_ParseCommand(char *cmd_line) {
  if (cmd_line[0] == '$') {
    if (cmd_line[1] == '$') {
      Config_PrintAll();
    } else if (strncmp(&cmd_line[1], "save", 4) == 0) {
      Config_Save();
    } else if (cmd_line[1] == 'h') {
      Config_PrintHelp();
    } else {
      // parse $x=y
      int param_id = -1;
      char *eq_ptr = strchr(cmd_line, '=');
      if (eq_ptr != NULL) {
        *eq_ptr = '\0';
        param_id = atoi(&cmd_line[1]);
        char *val_str = eq_ptr + 1;

        switch (param_id) {
        case 0:
          motor_config.pwm_freq = atoi(val_str);
          if (motor_config.pwm_freq < 1000) motor_config.pwm_freq = 1000;
          if (motor_config.pwm_freq > 100000) motor_config.pwm_freq = 100000;
          Config_ApplyPWMFrequency();
          break;
        case 1: motor_config.pole_pairs = atoi(val_str); break;
        case 2: motor_config.dc_bus_voltage = atof(val_str); break;
        case 3: motor_config.max_output_voltage = atof(val_str); break;
        case 4: motor_config.verbose_output = atoi(val_str); break;
        case 5: motor_config.verbose_period = atoi(val_str); break;
        case 6: motor_config.open_loop_voltage = atof(val_str); break;
        case 7: motor_config.accel_limit = atof(val_str); break;
        case 8: motor_config.switchover_rpm = atof(val_str); break;
        case 9: motor_config.switchover_delay = atof(val_str); break;
        case 10: motor_config.hall_offset_deg = atof(val_str); break;
        case 11: motor_config.vel_kp = atof(val_str); break;
        case 12: motor_config.vel_ki = atof(val_str); break;
        case 13: motor_config.accel_rpm_s = atof(val_str); break;
        default:
          cdc_printf("error: Invalid parameter\r\n");
          return;
        }

        cdc_printf("ok\r\n");
      } else {
        cdc_printf("error: Invalid command format\r\n");
      }
    }
  } else if (cmd_line[0] == 'S' || cmd_line[0] == 's') {
    float target = atof(&cmd_line[1]);
    SixStep_SetRPM(target);
    cdc_printf("ok\r\n");
  } else if (cmd_line[0] == 'T' || cmd_line[0] == 't') {
    SixStep_Stop();
    cdc_printf("Motor Stopped\r\n");
  } else {
    cdc_printf("error: Unknown command\r\n");
  }
}
