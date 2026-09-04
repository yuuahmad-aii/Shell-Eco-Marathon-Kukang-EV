#ifndef __MPU9250_H
#define __MPU9250_H

#include "main.h"

// MPU9250 Registers
#define MPU9250_REG_USER_CTRL     0x6A
#define MPU9250_REG_PWR_MGMT_1    0x6B
#define MPU9250_REG_CONFIG        0x1A
#define MPU9250_REG_GYRO_CONFIG   0x1B
#define MPU9250_REG_ACCEL_CONFIG  0x1C
#define MPU9250_REG_I2C_MST_CTRL  0x24
#define MPU9250_REG_I2C_SLV0_ADDR 0x25
#define MPU9250_REG_I2C_SLV0_REG  0x26
#define MPU9250_REG_I2C_SLV0_CTRL 0x27
#define MPU9250_REG_I2C_SLV0_DO   0x63

#define MPU9250_REG_ACCEL_XOUT_H  0x3B
#define MPU9250_REG_GYRO_XOUT_H   0x43
#define MPU9250_REG_EXT_SENS_DATA 0x49

// AK8963 Registers (Internal Magnetometer)
#define AK8963_I2C_ADDR           0x0C
#define AK8963_REG_CNTL1          0x0A
#define AK8963_REG_HXL            0x03

// Struct to store IMU data
typedef struct {
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    float mag_x, mag_y, mag_z;
} MPU9250_Data;

// Function Prototypes
uint8_t MPU9250_Init(SPI_HandleTypeDef *hspi);
uint8_t MPU9250_ReadID(SPI_HandleTypeDef *hspi);
void MPU9250_ReadSensor(SPI_HandleTypeDef *hspi, MPU9250_Data *data);

#endif /* __MPU9250_H */
