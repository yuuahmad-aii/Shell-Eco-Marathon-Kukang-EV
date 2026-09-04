#include "mpu9250.h"

#define MPU9250_CS_LOW()  HAL_GPIO_WritePin(SPI1_CS_IMU_GPIO_Port, SPI1_CS_IMU_Pin, GPIO_PIN_RESET)
#define MPU9250_CS_HIGH() HAL_GPIO_WritePin(SPI1_CS_IMU_GPIO_Port, SPI1_CS_IMU_Pin, GPIO_PIN_SET)

// Scales (assuming +-2g, +-250dps)
#define ACCEL_SCALE (2.0f / 32768.0f)
#define GYRO_SCALE  (250.0f / 32768.0f)

static void MPU9250_WriteReg(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t data) {
    uint8_t tx_data[2] = {reg & 0x7F, data};
    MPU9250_CS_LOW();
    HAL_SPI_Transmit(hspi, tx_data, 2, 100);
    MPU9250_CS_HIGH();
}

static void MPU9250_ReadRegs(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t *data, uint16_t len) {
    uint8_t tx = reg | 0x80; // MSB=1 for Read
    MPU9250_CS_LOW();
    HAL_SPI_Transmit(hspi, &tx, 1, 100);
    HAL_SPI_Receive(hspi, data, len, 100);
    MPU9250_CS_HIGH();
}

uint8_t MPU9250_ReadID(SPI_HandleTypeDef *hspi) {
    uint8_t id = 0;
    MPU9250_ReadRegs(hspi, 0x75, &id, 1);
    return id;
}

uint8_t MPU9250_Init(SPI_HandleTypeDef *hspi) {
    uint8_t id = 0;
    
    MPU9250_ReadRegs(hspi, 0x75, &id, 1); // WHO_AM_I
    if (id != 0x71 && id != 0x73) {
        return 0; // Invalid ID
    }

    // Wake up device
    MPU9250_WriteReg(hspi, MPU9250_REG_PWR_MGMT_1, 0x00);
    HAL_Delay(10);
    
    // Config gyro (+-250 dps) and accel (+-2g)
    MPU9250_WriteReg(hspi, MPU9250_REG_GYRO_CONFIG, 0x00);
    MPU9250_WriteReg(hspi, MPU9250_REG_ACCEL_CONFIG, 0x00);

    // Config I2C Master for AK8963
    MPU9250_WriteReg(hspi, MPU9250_REG_USER_CTRL, 0x20); // Enable I2C Master
    MPU9250_WriteReg(hspi, MPU9250_REG_I2C_MST_CTRL, 0x0D); // I2C clock 400kHz

    // Init AK8963 to 100Hz continuous mode, 16-bit
    MPU9250_WriteReg(hspi, MPU9250_REG_I2C_SLV0_ADDR, AK8963_I2C_ADDR);
    MPU9250_WriteReg(hspi, MPU9250_REG_I2C_SLV0_REG, AK8963_REG_CNTL1);
    MPU9250_WriteReg(hspi, MPU9250_REG_I2C_SLV0_DO, 0x16);
    MPU9250_WriteReg(hspi, MPU9250_REG_I2C_SLV0_CTRL, 0x81); // Enable I2C and write 1 byte
    HAL_Delay(10);

    // Setup AK8963 continuous read (7 bytes: HXL, HXH, HYL, HYH, HZL, HZH, ST2)
    MPU9250_WriteReg(hspi, MPU9250_REG_I2C_SLV0_ADDR, AK8963_I2C_ADDR | 0x80); // Read
    MPU9250_WriteReg(hspi, MPU9250_REG_I2C_SLV0_REG, AK8963_REG_HXL);
    MPU9250_WriteReg(hspi, MPU9250_REG_I2C_SLV0_CTRL, 0x87); // Enable I2C and read 7 bytes

    return 1;
}

void MPU9250_ReadSensor(SPI_HandleTypeDef *hspi, MPU9250_Data *data) {
    uint8_t raw_accel[6];
    uint8_t raw_gyro[6];
    uint8_t raw_mag[7];

    MPU9250_ReadRegs(hspi, MPU9250_REG_ACCEL_XOUT_H, raw_accel, 6);
    MPU9250_ReadRegs(hspi, MPU9250_REG_GYRO_XOUT_H, raw_gyro, 6);
    MPU9250_ReadRegs(hspi, MPU9250_REG_EXT_SENS_DATA, raw_mag, 7);

    int16_t ax = (raw_accel[0] << 8) | raw_accel[1];
    int16_t ay = (raw_accel[2] << 8) | raw_accel[3];
    int16_t az = (raw_accel[4] << 8) | raw_accel[5];

    int16_t gx = (raw_gyro[0] << 8) | raw_gyro[1];
    int16_t gy = (raw_gyro[2] << 8) | raw_gyro[3];
    int16_t gz = (raw_gyro[4] << 8) | raw_gyro[5];

    // AK8963 data is Little Endian
    int16_t mx = (raw_mag[1] << 8) | raw_mag[0];
    int16_t my = (raw_mag[3] << 8) | raw_mag[2];
    int16_t mz = (raw_mag[5] << 8) | raw_mag[4];

    data->accel_x = ax * ACCEL_SCALE;
    data->accel_y = ay * ACCEL_SCALE;
    data->accel_z = az * ACCEL_SCALE;

    data->gyro_x = gx * GYRO_SCALE;
    data->gyro_y = gy * GYRO_SCALE;
    data->gyro_z = gz * GYRO_SCALE;

    // Mag scale is roughly 0.15 uT/LSB
    data->mag_x = mx * 0.15f;
    data->mag_y = my * 0.15f;
    data->mag_z = mz * 0.15f;
}
