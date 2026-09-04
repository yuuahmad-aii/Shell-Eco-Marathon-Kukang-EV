#include "bmp280.h"
#include <math.h>

#define BMP280_CS_LOW()  HAL_GPIO_WritePin(SPI1_CS_BARO_GPIO_Port, SPI1_CS_BARO_Pin, GPIO_PIN_RESET)
#define BMP280_CS_HIGH() HAL_GPIO_WritePin(SPI1_CS_BARO_GPIO_Port, SPI1_CS_BARO_Pin, GPIO_PIN_SET)

static BMP280_CalibData calib;
static int32_t t_fine;

static void BMP280_WriteReg(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t data) {
    uint8_t tx_data[2] = {reg & 0x7F, data}; // MSB=0 for Write
    BMP280_CS_LOW();
    HAL_SPI_Transmit(hspi, tx_data, 2, 100);
    BMP280_CS_HIGH();
}

static void BMP280_ReadRegs(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t *data, uint16_t len) {
    uint8_t tx = reg | 0x80; // MSB=1 for Read
    BMP280_CS_LOW();
    HAL_SPI_Transmit(hspi, &tx, 1, 100);
    HAL_SPI_Receive(hspi, data, len, 100);
    BMP280_CS_HIGH();
}

uint8_t BMP280_ReadID(SPI_HandleTypeDef *hspi) {
    uint8_t id = 0;
    BMP280_ReadRegs(hspi, BMP280_REG_ID, &id, 1);
    return id;
}

uint8_t BMP280_Init(SPI_HandleTypeDef *hspi) {
    uint8_t id = 0;
    
    BMP280_ReadRegs(hspi, BMP280_REG_ID, &id, 1);
    if (id != 0x58) {
        return 0; // BMP280 ID is typically 0x58
    }

    // Read Calibration Data
    uint8_t calib_buf[24];
    BMP280_ReadRegs(hspi, BMP280_REG_CALIB_START, calib_buf, 24);

    calib.dig_T1 = (calib_buf[1] << 8) | calib_buf[0];
    calib.dig_T2 = (calib_buf[3] << 8) | calib_buf[2];
    calib.dig_T3 = (calib_buf[5] << 8) | calib_buf[4];
    calib.dig_P1 = (calib_buf[7] << 8) | calib_buf[6];
    calib.dig_P2 = (calib_buf[9] << 8) | calib_buf[8];
    calib.dig_P3 = (calib_buf[11] << 8) | calib_buf[10];
    calib.dig_P4 = (calib_buf[13] << 8) | calib_buf[12];
    calib.dig_P5 = (calib_buf[15] << 8) | calib_buf[14];
    calib.dig_P6 = (calib_buf[17] << 8) | calib_buf[16];
    calib.dig_P7 = (calib_buf[19] << 8) | calib_buf[18];
    calib.dig_P8 = (calib_buf[21] << 8) | calib_buf[20];
    calib.dig_P9 = (calib_buf[23] << 8) | calib_buf[22];

    // Configure sensor: Normal mode, Temp oversampling x1, Press oversampling x1
    BMP280_WriteReg(hspi, BMP280_REG_CTRL_MEAS, 0x27);
    
    // Config: Standby 1000ms, Filter off
    BMP280_WriteReg(hspi, BMP280_REG_CONFIG, 0xA0);

    return 1;
}

static float BMP280_CompensateTemp(int32_t adc_T) {
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
    t_fine = var1 + var2;
    float T = (t_fine * 5 + 128) >> 8;
    return T / 100.0f;
}

static float BMP280_CompensatePress(int32_t adc_P) {
    int64_t var1, var2, p;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib.dig_P3) >> 8) + ((var1 * (int64_t)calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_P1) >> 33;

    if (var1 == 0) return 0; // Avoid division by zero

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib.dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)calib.dig_P7) << 4);
    return (float)p / 256.0f;
}

void BMP280_ReadSensor(SPI_HandleTypeDef *hspi, BMP280_Data *data) {
    uint8_t raw[6];
    BMP280_ReadRegs(hspi, BMP280_REG_PRESS_MSB, raw, 6);

    int32_t adc_P = (raw[0] << 12) | (raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = (raw[3] << 12) | (raw[4] << 4) | (raw[5] >> 4);

    data->temperature = BMP280_CompensateTemp(adc_T);
    data->pressure = BMP280_CompensatePress(adc_P);
    
    // Calculate approximate altitude (based on standard sea level pressure 1013.25 hPa)
    data->altitude = 44330.0f * (1.0f - powf(data->pressure / 101325.0f, 0.1903f));
}
