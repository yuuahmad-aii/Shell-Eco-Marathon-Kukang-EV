#include "config.h"
#include "hub_motor_ebike.h"
#if (CONTROL_ALGORITHM == CONTROL_FOC)
#include "foc.h"
#elif (CONTROL_ALGORITHM == CONTROL_SIX_STEP)
#include "six_step.h"
#endif
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

  // Copy floats into buffer (assuming little-endian architecture like
  // Cortex-M4)
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

  // Non-blocking transmit: if USBD is busy, we just drop this frame to keep FOC
  // loop fast!
  CDC_Transmit_FS(tx_buffer, 33);
}

void Config_LoadDefaults(void) {
  motor_config.magic_word = CONFIG_MAGIC_WORD;
  motor_config.pwm_freq = 20000;
  motor_config.pole_pairs = 7;     // Typical for small BLDC
  motor_config.magnets = 14;       // Typical for small BLDC
  motor_config.encoder_ppr = 2500; // Updated as requested
  motor_config.dc_bus_voltage = 24.0f;
  motor_config.max_output_voltage = 24.0f;

  // Default Control Mode
  motor_config.control_mode = 1; // Velocity mode by default

  // Default Velocity PID
  motor_config.vel_kp = 0.5f;
  motor_config.vel_ki = 0.1f;
  motor_config.vel_kd = 0.0f;
  motor_config.vel_limit = motor_config.max_output_voltage;

  // Default Position PID
  motor_config.pos_kp = 2.0f;
  motor_config.pos_ki = 0.0f;
  motor_config.pos_kd = 0.1f;
  motor_config.pos_limit = 50.0f; // Max target velocity in rad/s

  motor_config.encoder_dir = 0;          // 0=Normal, 1=Inverted
  motor_config.verbose_output = 1;       // Enable GUI telemetry by default
  motor_config.verbose_period = 10;      // 10ms (100Hz) high-speed telemetry
  motor_config.open_loop_voltage = 2.0f; // 2.0V safe voltage
  motor_config.accel_limit = 50.0f; // Default 50 rad/s^2 acceleration limit

  // Default Current PID
  motor_config.current_kp = 0.5f;
  motor_config.current_ki = 0.1f;
  motor_config.current_limit = motor_config.dc_bus_voltage;

  // Default current scaling (BTS7960: IS pin ~1/8500 ratio)
  motor_config.current_sense_gain = 0.0068f;
  motor_config.current_sense_offset_a = 0.0f;
  motor_config.current_sense_offset_b = 0.0f;
  motor_config.current_sense_offset_c = 0.0f;

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
  // For STM32G431CB (128KB), page 63 is the last page.
  // Address 0x0801F800 is Page 63
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
    // Write double words (64 bit) for STM32G4 flash if using standard
    // HAL_FLASH_Program Since we write 64-bit at a time, ensure structure size
    // is multiple of 8 bytes. motor_config_t size is currently 32 bytes + 40
    // bytes = 72 bytes (divisible by 8)
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
  // Timer is Center Aligned, Prescaler = 0.
  // Period = (SystemCoreClock / pwm_freq) / 2

  uint32_t period = (HAL_RCC_GetHCLKFreq() / motor_config.pwm_freq) / 2;

  htim1.Instance->ARR = period - 1; // Update auto-reload register
}

void Config_PrintAll(void) {
  int bus_v_int = (int)motor_config.dc_bus_voltage;
  int bus_v_frac = (int)(motor_config.dc_bus_voltage * 100.0f) % 100;
  if (bus_v_frac < 0)
    bus_v_frac = -bus_v_frac;

  int max_v_int = (int)motor_config.max_output_voltage;
  int max_v_frac = (int)(motor_config.max_output_voltage * 100.0f) % 100;
  if (max_v_frac < 0)
    max_v_frac = -max_v_frac;

  int vkp_i = (int)motor_config.vel_kp,
      vkp_f = (int)(motor_config.vel_kp * 100) % 100;
  if (vkp_f < 0)
    vkp_f = -vkp_f;
  int vki_i = (int)motor_config.vel_ki,
      vki_f = (int)(motor_config.vel_ki * 100) % 100;
  if (vki_f < 0)
    vki_f = -vki_f;
  int vkd_i = (int)motor_config.vel_kd,
      vkd_f = (int)(motor_config.vel_kd * 100) % 100;
  if (vkd_f < 0)
    vkd_f = -vkd_f;
  int vlim_i = (int)motor_config.vel_limit,
      vlim_f = (int)(motor_config.vel_limit * 100) % 100;
  if (vlim_f < 0)
    vlim_f = -vlim_f;

  int pkp_i = (int)motor_config.pos_kp,
      pkp_f = (int)(motor_config.pos_kp * 100) % 100;
  if (pkp_f < 0)
    pkp_f = -pkp_f;
  int pki_i = (int)motor_config.pos_ki,
      pki_f = (int)(motor_config.pos_ki * 100) % 100;
  if (pki_f < 0)
    pki_f = -pki_f;
  int pkd_i = (int)motor_config.pos_kd,
      pkd_f = (int)(motor_config.pos_kd * 100) % 100;
  if (pkd_f < 0)
    pkd_f = -pkd_f;
  int plim_i = (int)motor_config.pos_limit,
      plim_f = (int)(motor_config.pos_limit * 100) % 100;
  if (plim_f < 0)
    plim_f = -plim_f;

  int olv_i = (int)motor_config.open_loop_voltage,
      olv_f = (int)(motor_config.open_loop_voltage * 100) % 100;
  if (olv_f < 0)
    olv_f = -olv_f;
  int acc_i = (int)motor_config.accel_limit,
      acc_f = (int)(motor_config.accel_limit * 100) % 100;
  if (acc_f < 0)
    acc_f = -acc_f;

  int ikp_i = (int)motor_config.current_kp,
      ikp_f = (int)(motor_config.current_kp * 100) % 100;
  if (ikp_f < 0)
    ikp_f = -ikp_f;
  int iki_i = (int)motor_config.current_ki,
      iki_f = (int)(motor_config.current_ki * 100) % 100;
  if (iki_f < 0)
    iki_f = -iki_f;
  int ilim_i = (int)motor_config.current_limit,
      ilim_f = (int)(motor_config.current_limit * 100) % 100;
  if (ilim_f < 0)
    ilim_f = -ilim_f;
  int igain_i = (int)motor_config.current_sense_gain,
      igain_f = (int)(motor_config.current_sense_gain * 10000) % 10000;
  if (igain_f < 0)
    igain_f = -igain_f;

  int ofs_a_i = (int)motor_config.current_sense_offset_a,
      ofs_a_f = (int)(motor_config.current_sense_offset_a * 100) % 100;
  if (ofs_a_f < 0)
    ofs_a_f = -ofs_a_f;
  int ofs_b_i = (int)motor_config.current_sense_offset_b,
      ofs_b_f = (int)(motor_config.current_sense_offset_b * 100) % 100;
  if (ofs_b_f < 0)
    ofs_b_f = -ofs_b_f;
  int ofs_c_i = (int)motor_config.current_sense_offset_c,
      ofs_c_f = (int)(motor_config.current_sense_offset_c * 100) % 100;
  if (ofs_c_f < 0)
    ofs_c_f = -ofs_c_f;

  cdc_printf("\r\n--- GRBL Config ---\r\n");
  cdc_printf("$0=%lu (PWM Freq Hz)\r\n", motor_config.pwm_freq);
  cdc_printf("$1=%lu (Pole Pairs)\r\n", motor_config.pole_pairs);
  cdc_printf("$2=%lu (Magnets)\r\n", motor_config.magnets);
  cdc_printf("$3=%lu (Encoder PPR)\r\n", motor_config.encoder_ppr);
  cdc_printf("$4=%d.%02d (DC Bus Voltage)\r\n", bus_v_int, bus_v_frac);
  cdc_printf("$5=%d.%02d (Max Output Voltage)\r\n", max_v_int, max_v_frac);

  cdc_printf("$6=%lu (Control Mode: 0=Torque,1=Vel,2=Pos,3=Open)\r\n",
             motor_config.control_mode);
  cdc_printf("$7=%d.%02d (Vel Kp)\r\n", vkp_i, vkp_f);
  cdc_printf("$8=%d.%02d (Vel Ki)\r\n", vki_i, vki_f);
  cdc_printf("$9=%d.%02d (Vel Kd)\r\n", vkd_i, vkd_f);
  cdc_printf("$10=%d.%02d (Vel Limit)\r\n", vlim_i, vlim_f);

  cdc_printf("$11=%d.%02d (Pos Kp)\r\n", pkp_i, pkp_f);
  cdc_printf("$12=%d.%02d (Pos Ki)\r\n", pki_i, pki_f);
  cdc_printf("$13=%d.%02d (Pos Kd)\r\n", pkd_i, pkd_f);
  cdc_printf("$14=%d.%02d (Pos Limit)\r\n", plim_i, plim_f);
  cdc_printf("$15=%lu (Encoder Dir: 0=Normal, 1=Inv)\r\n",
             motor_config.encoder_dir);
  cdc_printf("$16=%lu (Verbose: 0=Off, 1=On)\r\n", motor_config.verbose_output);
  cdc_printf("$17=%lu (Verbose Period ms)\r\n", motor_config.verbose_period);
  cdc_printf("$18=%d.%02d (Open Loop Voltage V)\r\n", olv_i, olv_f);
  cdc_printf("$19=%d.%02d (Accel Limit rad/s^2)\r\n", acc_i, acc_f);
  cdc_printf("$20=%d.%02d (Curr Kp)\r\n", ikp_i, ikp_f);
  cdc_printf("$21=%d.%02d (Curr Ki)\r\n", iki_i, iki_f);
  cdc_printf("$22=%d.%02d (Curr Limit)\r\n", ilim_i, ilim_f);
  cdc_printf("$23=%d.%04d (Curr Sense Gain)\r\n", igain_i, igain_f);
  cdc_printf("$24=%d.%02d (Curr Offset A)\r\n", ofs_a_i, ofs_a_f);
  cdc_printf("$25=%d.%02d (Curr Offset B)\r\n", ofs_b_i, ofs_b_f);
  cdc_printf("$26=%d.%02d (Curr Offset C)\r\n", ofs_c_i, ofs_c_f);
  cdc_printf("ok\r\n");
}

void Config_PrintStatus(void) {
#if (CONTROL_ALGORITHM == CONTROL_FOC)
  int p_i = (int)foc.mechanical_angle,
      p_f = (int)(foc.mechanical_angle * 100) % 100;
  if (p_f < 0)
    p_f = -p_f;
  int v_i = (int)foc.shaft_velocity,
      v_f = (int)(foc.shaft_velocity * 100) % 100;
  if (v_f < 0)
    v_f = -v_f;
  int vq_i = (int)foc.v_q, vq_f = (int)(foc.v_q * 100) % 100;
  if (vq_f < 0)
    vq_f = -vq_f;

  cdc_printf("Angle: %d.%02d rad\r\n", p_i, p_f);
  cdc_printf("Velocity: %d.%02d rad/s\r\n", v_i, v_f);
  cdc_printf("Voltage Q: %d.%02d V\r\n", vq_i, vq_f);
#elif (CONTROL_ALGORITHM == CONTROL_SIX_STEP)
  SixStep_PrintVerbose();
#endif
  cdc_printf("ok\r\n");
}

void Config_PrintHelp(void) {
  cdc_printf("\r\n--- Commands ---\r\n");
  cdc_printf("$$      : Show all settings\r\n");
  cdc_printf("?       : Show current status\r\n");
  cdc_printf("$x=y    : Set parameter x to value y\r\n");
  cdc_printf("$save   : Save settings to flash\r\n");
  cdc_printf("$h      : Show this help\r\n");
  cdc_printf("S<val>  : Set Target (Meaning depends on $6)\r\n");
  cdc_printf(
      "          ($6=0: Volts, $6=1: Rad/s, $6=2: Rad, $6=3: Rad/s)\r\n");
  cdc_printf("ok\r\n");
}

void Config_ParseCommand(char *cmd_line) {
  if (cmd_line[0] == '$') {
    if (cmd_line[1] == '$') {
      Config_PrintAll();
    } else if (strncmp(&cmd_line[1], "save", 4) == 0) {
      Config_Save();
    } else if (strncmp(&cmd_line[1], "align", 5) == 0) {
#if (CONTROL_ALGORITHM == CONTROL_FOC)
      FOC_Align();
      cdc_printf("Aligning motor... please wait 2 seconds\r\n");
#else
      cdc_printf("error: Align only valid in FOC mode\r\n");
#endif
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
          if (motor_config.pwm_freq < 1000)
            motor_config.pwm_freq = 1000;
          if (motor_config.pwm_freq > 100000)
            motor_config.pwm_freq = 100000;
          Config_ApplyPWMFrequency();
          break;
        case 1:
          motor_config.pole_pairs = atoi(val_str);
          break;
        case 2:
          motor_config.magnets = atoi(val_str);
          break;
        case 3:
          motor_config.encoder_ppr = atoi(val_str);
          break;
        case 4:
          motor_config.dc_bus_voltage = atof(val_str);
          break;
        case 5:
          motor_config.max_output_voltage = atof(val_str);
          break;
        case 6:
          motor_config.control_mode = atoi(val_str);
          break;
        case 7:
          motor_config.vel_kp = atof(val_str);
          break;
        case 8:
          motor_config.vel_ki = atof(val_str);
          break;
        case 9:
          motor_config.vel_kd = atof(val_str);
          break;
        case 10:
          motor_config.vel_limit = atof(val_str);
          break;
        case 11:
          motor_config.pos_kp = atof(val_str);
          break;
        case 12:
          motor_config.pos_ki = atof(val_str);
          break;
        case 13:
          motor_config.pos_kd = atof(val_str);
          break;
        case 14:
          motor_config.pos_limit = atof(val_str);
          break;
        case 15:
          motor_config.encoder_dir = atoi(val_str);
          break;
        case 16:
          motor_config.verbose_output = atoi(val_str);
          break;
        case 17:
          motor_config.verbose_period = atoi(val_str);
          break;
        case 18:
          motor_config.open_loop_voltage = atof(val_str);
          break;
        case 19:
          motor_config.accel_limit = atof(val_str);
          break;
        case 20:
          motor_config.current_kp = atof(val_str);
          break;
        case 21:
          motor_config.current_ki = atof(val_str);
          break;
        case 22:
          motor_config.current_limit = atof(val_str);
          break;
        case 23:
          motor_config.current_sense_gain = atof(val_str);
          break;
        case 24:
          motor_config.current_sense_offset_a = atof(val_str);
          break;
        case 25:
          motor_config.current_sense_offset_b = atof(val_str);
          break;
        case 26:
          motor_config.current_sense_offset_c = atof(val_str);
          break;
        default:
          cdc_printf("error: Invalid parameter\r\n");
          return;
        }

        // Update PID variables immediately
#if (CONTROL_ALGORITHM == CONTROL_FOC)
        extern pid_controller_t pid_id, pid_iq;
        pid_vel.kp = motor_config.vel_kp;
        pid_vel.ki = motor_config.vel_ki;
        pid_vel.kd = motor_config.vel_kd;
        pid_vel.output_limit = motor_config.vel_limit;
        pid_pos.kp = motor_config.pos_kp;
        pid_pos.ki = motor_config.pos_ki;
        pid_pos.kd = motor_config.pos_kd;
        pid_pos.output_limit = motor_config.pos_limit;
        pid_id.kp = motor_config.current_kp;
        pid_id.ki = motor_config.current_ki;
        pid_id.output_limit = motor_config.current_limit;
        pid_iq.kp = motor_config.current_kp;
        pid_iq.ki = motor_config.current_ki;
        pid_iq.output_limit = motor_config.current_limit;
#endif

        cdc_printf("ok\r\n");
      } else {
        cdc_printf("error: Invalid command format\r\n");
      }
    }
  } else if (cmd_line[0] == '?') {
    Config_PrintStatus();
  } else if (cmd_line[0] == 'S' || cmd_line[0] == 's') {
    float target = atof(&cmd_line[1]);
#if (CONTROL_ALGORITHM == CONTROL_FOC)
    FOC_SetTarget(target);
    extern void enable_mosfet(void);
    enable_mosfet();
#elif (CONTROL_ALGORITHM == CONTROL_SIX_STEP)
    SixStep_SetSpeed(target);
#endif
    cdc_printf("ok\r\n");
  } else if (cmd_line[0] == 'T' || cmd_line[0] == 't') {
#if (CONTROL_ALGORITHM == CONTROL_FOC)
    extern void disable_mosfet(void);
    disable_mosfet();
#elif (CONTROL_ALGORITHM == CONTROL_SIX_STEP)
    SixStep_Stop();
#endif
    cdc_printf("Motor Disabled\r\n");
  } else {
    cdc_printf("error: Unknown command\r\n");
  }
}
