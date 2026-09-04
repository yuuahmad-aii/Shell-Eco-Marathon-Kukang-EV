#ifndef __BMP280_H
#define __BMP280_H

#include "main.h"

// BMP280 Registers
#define BMP280_REG_TEMP_XLSB  0xFC
#define BMP280_REG_TEMP_LSB   0xFB
#define BMP280_REG_TEMP_MSB   0xFA
#define BMP280_REG_PRESS_XLSB 0xF9
#define BMP280_REG_PRESS_LSB  0xF8
#define BMP280_REG_PRESS_MSB  0xF7
#define BMP280_REG_CONFIG     0xF5
#define BMP280_REG_CTRL_MEAS  0xF4
#define BMP280_REG_STATUS     0xF3
#define BMP280_REG_RESET      0xE0
#define BMP280_REG_ID         0xD0
#define BMP280_REG_CALIB_START 0x88

// Struct to store calibration parameters
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
} BMP280_CalibData;

// Struct to store sensor data
typedef struct {
    float temperature; // In Celsius
    float pressure;    // In Pa
    float altitude;    // In Meters (approximate)
} BMP280_Data;

// Function Prototypes
uint8_t BMP280_Init(SPI_HandleTypeDef *hspi);
uint8_t BMP280_ReadID(SPI_HandleTypeDef *hspi);
void BMP280_ReadSensor(SPI_HandleTypeDef *hspi, BMP280_Data *data);

#endif /* __BMP280_H */
