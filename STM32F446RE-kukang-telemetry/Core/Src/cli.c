#include "cli.h"
#include "config.h"
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "bmp280.h"
#include "mpu9250.h"
#include "gps_neo6m.h"
#include "speed.h"

extern SPI_HandleTypeDef hspi1;

#define CLI_BUFFER_SIZE 64
static char cli_buffer[CLI_BUFFER_SIZE];
static uint8_t cli_index = 0;
volatile uint8_t cli_command_ready = 0;

#include <stdarg.h>

void CLI_Print(const char *format, ...) {
    static char tx_buffer[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(tx_buffer, sizeof(tx_buffer), format, args);
    va_end(args);

    if (len > 0) {
        uint32_t timeout = 0xFFFF;
        while (CDC_Transmit_FS((uint8_t *)tx_buffer, len) == USBD_BUSY && timeout > 0) {
            timeout--;
        }
    }
}

void CLI_Init(void) {
    cli_index = 0;
}

static void CLI_ParseCommand(void) {
    if (cli_index == 0) return;
    
    if (strcmp(cli_buffer, "$$") == 0) {
        int v_i, v_f;
        
        v_i = (int)current_config.wheel_diameter_mm;
        v_f = (int)(current_config.wheel_diameter_mm * 100) % 100; if(v_f < 0) v_f = -v_f;
        CLI_Print("$10=%d.%02d (Wheel Diameter mm)\r\n", v_i, v_f);
        
        v_i = (int)current_config.pulses_per_rev;
        v_f = (int)(current_config.pulses_per_rev * 100) % 100; if(v_f < 0) v_f = -v_f;
        CLI_Print("$11=%d.%02d (Pulses Per Rev)\r\n", v_i, v_f);
        
        v_i = (int)current_config.coast_speed_min;
        v_f = (int)(current_config.coast_speed_min * 100) % 100; if(v_f < 0) v_f = -v_f;
        CLI_Print("$20=%d.%02d (Coast Speed Min km/h)\r\n", v_i, v_f);
        
        v_i = (int)current_config.burn_speed_max;
        v_f = (int)(current_config.burn_speed_max * 100) % 100; if(v_f < 0) v_f = -v_f;
        CLI_Print("$21=%d.%02d (Burn Speed Max km/h)\r\n", v_i, v_f);
        
        CLI_Print("ok\r\n");
    }
    else if (strcmp(cli_buffer, "$?") == 0) {
        BMP280_Data bmp;
        MPU9250_Data imu;
        GPS_Data gps;
        
        BMP280_ReadSensor(&hspi1, &bmp);
        MPU9250_ReadSensor(&hspi1, &imu);
        GPS_GetLatestData(&gps);
        
        int v_i, v_f;
        int vy_i, vy_f, vz_i, vz_f;
        
        // Accel
        v_i = (int)imu.accel_x; v_f = (int)(imu.accel_x * 100) % 100; if(v_f<0) v_f=-v_f;
        vy_i = (int)imu.accel_y; vy_f = (int)(imu.accel_y * 100) % 100; if(vy_f<0) vy_f=-vy_f;
        vz_i = (int)imu.accel_z; vz_f = (int)(imu.accel_z * 100) % 100; if(vz_f<0) vz_f=-vz_f;
        CLI_Print("Accel(g): X:%d.%02d Y:%d.%02d Z:%d.%02d\r\n", v_i, v_f, vy_i, vy_f, vz_i, vz_f);
        
        // Gyro
        v_i = (int)imu.gyro_x; v_f = (int)(imu.gyro_x * 10) % 10; if(v_f<0) v_f=-v_f;
        vy_i = (int)imu.gyro_y; vy_f = (int)(imu.gyro_y * 10) % 10; if(vy_f<0) vy_f=-vy_f;
        vz_i = (int)imu.gyro_z; vz_f = (int)(imu.gyro_z * 10) % 10; if(vz_f<0) vz_f=-vz_f;
        CLI_Print("Gyro(dps): X:%d.%01d Y:%d.%01d Z:%d.%01d\r\n", v_i, v_f, vy_i, vy_f, vz_i, vz_f);
        
        // Baro
        v_i = (int)bmp.altitude; v_f = (int)(bmp.altitude * 100) % 100; if(v_f<0) v_f=-v_f;
        CLI_Print("Baro Alt(m): %d.%02d\r\n", v_i, v_f);
        
        // GPS
        v_i = (int)gps.latitude; v_f = (int)(gps.latitude * 1000000) % 1000000; if(v_f<0) v_f=-v_f;
        vy_i = (int)gps.longitude; vy_f = (int)(gps.longitude * 1000000) % 1000000; if(vy_f<0) vy_f=-vy_f;
        CLI_Print("GPS: %d.%06d, %d.%06d (Sats:%d Fix:%d)\r\n", v_i, v_f, vy_i, vy_f, gps.num_satellites, gps.fix_type);
        
        if (gps.is_time_valid) {
            CLI_Print("GPS Time: %04d-%02d-%02d %02d:%02d:%02d UTC\r\n", gps.year, gps.month, gps.day, gps.hour, gps.min, gps.sec);
        } else {
            CLI_Print("GPS Time: Invalid/No Fix\r\n");
        }
        
        // Speed
        v_i = (int)speed_left_kmh; v_f = (int)(speed_left_kmh * 100) % 100; if(v_f<0) v_f=-v_f;
        vy_i = (int)speed_right_kmh; vy_f = (int)(speed_right_kmh * 100) % 100; if(vy_f<0) vy_f=-vy_f;
        CLI_Print("Speed(km/h): L:%d.%02d R:%d.%02d\r\n", v_i, v_f, vy_i, vy_f);
        
        CLI_Print("ok\r\n");
    } 
    else if (strcmp(cli_buffer, "$i") == 0) {
        CLI_Print("--- Firmware Info ---\r\n");
        CLI_Print("MCU            : STM32F446RE (Core M4)\r\n");
        CLI_Print("Datalogger Ver : 1.0.0\r\n");
        CLI_Print("Author         : Yuuahmad+Gemini\r\n");
        
        uint8_t imu_id = MPU9250_ReadID(&hspi1);
        char imu_name[16] = "Unknown";
        if (imu_id == 0x71) strcpy(imu_name, "MPU9250");
        else if (imu_id == 0x73) strcpy(imu_name, "MPU9255");
        else if (imu_id == 0x70) strcpy(imu_name, "MPU6500");
        
        CLI_Print("IMU Sensor     : 0x%02X (%s) (SPI1)\r\n", imu_id, imu_name);
        
        uint8_t baro_id = BMP280_ReadID(&hspi1);
        char baro_name[16] = "Unknown";
        if (baro_id == 0x58) strcpy(baro_name, "BMP280");
        else if (baro_id == 0x60) strcpy(baro_name, "BME280");
        else if (baro_id == 0x55) strcpy(baro_name, "BMP180");
        
        CLI_Print("Baro Sensor    : 0x%02X (%s) (SPI1)\r\n", baro_id, baro_name);
        
        GPS_Data gps;
        GPS_GetLatestData(&gps);
        
        if (strlen(gps.sw_version) > 0) {
            CLI_Print("GPS Sensor     : UBX Protocol (UART1)\r\n");
            CLI_Print("GPS Firmware   : SW: %s, HW: %s\r\n", gps.sw_version, gps.hw_version);
        } else {
            CLI_Print("GPS Sensor     : UBX Protocol (NEO-6M/7M) (UART1)\r\n");
            CLI_Print("GPS Firmware   : Unknown (Waiting for UBX-MON-VER)\r\n");
        }
        
        CLI_Print("ok\r\n");
    }
    else if (strcmp(cli_buffer, "$help") == 0) {
        CLI_Print("Available commands:\r\n");
        CLI_Print("$$    - View all parameters\r\n");
        CLI_Print("$?    - View all sensors\r\n");
        CLI_Print("$i    - View firmware info\r\n");
        CLI_Print("$x=y  - Set parameter x to value y\r\n");
        CLI_Print("$save - Save config to Flash\r\n");
        CLI_Print("ok\r\n");
    }
    else if (strcmp(cli_buffer, "$save") == 0) {
        Config_Save();
        CLI_Print("Config saved to flash.\r\n");
        CLI_Print("ok\r\n");
    }
    else if (cli_buffer[0] == '$') {
        // Parse $x=y
        int param_id = -1;
        char *eq_ptr = strchr(cli_buffer, '=');
        if (eq_ptr != NULL) {
            *eq_ptr = '\0';
            param_id = atoi(&cli_buffer[1]);
            char *val_str = eq_ptr + 1;
            float value = atof(val_str);
            
            switch (param_id) {
                case 10: current_config.wheel_diameter_mm = value; CLI_Print("ok\r\n"); break;
                case 11: current_config.pulses_per_rev = value; CLI_Print("ok\r\n"); break;
                case 20: current_config.coast_speed_min = value; CLI_Print("ok\r\n"); break;
                case 21: current_config.burn_speed_max = value; CLI_Print("ok\r\n"); break;
                default: CLI_Print("error: unknown parameter\r\n"); break;
            }
        } else {
            CLI_Print("error: invalid format\r\n");
        }
    } else {
        CLI_Print("error: unrecognized command\r\n");
    }
}

void CLI_Task(void) {
    if (cli_command_ready) {
        CLI_ParseCommand();
        cli_index = 0;
        cli_command_ready = 0;
    }
}

// Call this from HAL_UART_RxCpltCallback or CDC_Receive_FS
void CLI_ProcessChar(char c) {
    if (cli_command_ready) return; // Drop until processed

    if (c == '\r' || c == '\n') {
        if (cli_index > 0) {
            cli_buffer[cli_index] = '\0'; // Null terminate
            cli_command_ready = 1; // Hand over to main loop
        }
    } 
    else if (c == '\b' || c == 127) { // Backspace
        if (cli_index > 0) {
            cli_index--;
        }
    }
    else if (cli_index < CLI_BUFFER_SIZE - 1) {
        cli_buffer[cli_index++] = c;
    }
}

// Global hook for UART callback
// Place this inside HAL_UART_RxCpltCallback in main.c
// if (huart->Instance == USART2) {
//     extern uint8_t cli_rx_char;
//     CLI_ProcessChar(cli_rx_char);
// }
