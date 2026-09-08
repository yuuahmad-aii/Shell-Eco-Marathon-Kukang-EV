#include "ds18b20.h"

extern TIM_HandleTypeDef htim14;

// Microsecond delay using TIM14
void delay_us(uint16_t us) {
    __HAL_TIM_SET_COUNTER(&htim14, 0);
    while (__HAL_TIM_GET_COUNTER(&htim14) < us);
}

// 1-Wire Primitives
static uint8_t OW_Reset(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    uint8_t response = 0;
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
    delay_us(480);
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
    delay_us(80);
    if (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == GPIO_PIN_RESET) {
        response = 1;
    }
    delay_us(400);
    return response;
}

static void OW_WriteBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t bit) {
    if (bit) {
        HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
        delay_us(1);
        HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
        delay_us(60);
    } else {
        HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
        delay_us(60);
        HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
        delay_us(1); // Give line time to recover
    }
}

static uint8_t OW_ReadBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    uint8_t bit = 0;
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
    delay_us(2);
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
    delay_us(10);
    if (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == GPIO_PIN_SET) {
        bit = 1;
    }
    delay_us(50);
    return bit;
}

static void OW_WriteByte(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        OW_WriteBit(GPIOx, GPIO_Pin, data & 0x01);
        data >>= 1;
    }
}

static uint8_t OW_ReadByte(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    uint8_t data = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (OW_ReadBit(GPIOx, GPIO_Pin)) {
            data |= (1 << i);
        }
    }
    return data;
}

// Global search state
static uint8_t ROM_NO[8];
static uint8_t LastDiscrepancy;
static uint8_t LastFamilyDiscrepancy;
static uint8_t LastDeviceFlag;

static uint8_t OW_Search(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t *newAddr) {
    uint8_t id_bit_number;
    uint8_t last_zero, rom_byte_number, search_result;
    uint8_t id_bit, cmp_id_bit;
    uint8_t rom_byte_mask, search_direction;

    id_bit_number = 1;
    last_zero = 0;
    rom_byte_number = 0;
    rom_byte_mask = 1;
    search_result = 0;

    if (!LastDeviceFlag) {
        if (!OW_Reset(GPIOx, GPIO_Pin)) {
            LastDiscrepancy = 0;
            LastDeviceFlag = 0;
            LastFamilyDiscrepancy = 0;
            return 0;
        }

        OW_WriteByte(GPIOx, GPIO_Pin, DS18B20_CMD_SEARCHROM);

        do {
            id_bit = OW_ReadBit(GPIOx, GPIO_Pin);
            cmp_id_bit = OW_ReadBit(GPIOx, GPIO_Pin);

            if ((id_bit == 1) && (cmp_id_bit == 1)) {
                break;
            } else {
                if (id_bit != cmp_id_bit) {
                    search_direction = id_bit;
                } else {
                    if (id_bit_number < LastDiscrepancy) {
                        search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
                    } else {
                        search_direction = (id_bit_number == LastDiscrepancy);
                    }

                    if (search_direction == 0) {
                        last_zero = id_bit_number;
                        if (last_zero < 9)
                            LastFamilyDiscrepancy = last_zero;
                    }
                }

                if (search_direction == 1)
                    ROM_NO[rom_byte_number] |= rom_byte_mask;
                else
                    ROM_NO[rom_byte_number] &= ~rom_byte_mask;

                OW_WriteBit(GPIOx, GPIO_Pin, search_direction);
                id_bit_number++;
                rom_byte_mask <<= 1;

                if (rom_byte_mask == 0) {
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while (rom_byte_number < 8);

        if (!(id_bit_number < 65)) {
            LastDiscrepancy = last_zero;
            if (LastDiscrepancy == 0)
                LastDeviceFlag = 1;
            search_result = 1;
        }
    }

    if (!search_result || !ROM_NO[0]) {
        LastDiscrepancy = 0;
        LastDeviceFlag = 0;
        LastFamilyDiscrepancy = 0;
        search_result = 0;
    } else {
        for (int i = 0; i < 8; i++) newAddr[i] = ROM_NO[i];
    }
    return search_result;
}

uint8_t DS18B20_Search(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, DS18B20_Device *devices, uint8_t max_devices) {
    uint8_t num_found = 0;
    LastDiscrepancy = 0;
    LastDeviceFlag = 0;
    LastFamilyDiscrepancy = 0;

    while (num_found < max_devices && OW_Search(GPIOx, GPIO_Pin, devices[num_found].rom_code)) {
        devices[num_found].temperature = 0.0f;
        num_found++;
    }
    return num_found;
}

void DS18B20_StartAll(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    if (OW_Reset(GPIOx, GPIO_Pin)) {
        OW_WriteByte(GPIOx, GPIO_Pin, DS18B20_CMD_SKIPROM);
        OW_WriteByte(GPIOx, GPIO_Pin, DS18B20_CMD_CONVERT);
    }
}

float DS18B20_ReadTemp(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t *rom_code) {
    if (!OW_Reset(GPIOx, GPIO_Pin)) return -999.0f;
    
    OW_WriteByte(GPIOx, GPIO_Pin, DS18B20_CMD_MATCHROM);
    for (int i = 0; i < 8; i++) {
        OW_WriteByte(GPIOx, GPIO_Pin, rom_code[i]);
    }
    
    OW_WriteByte(GPIOx, GPIO_Pin, DS18B20_CMD_READSCRATCH);
    
    uint8_t lsb = OW_ReadByte(GPIOx, GPIO_Pin);
    uint8_t msb = OW_ReadByte(GPIOx, GPIO_Pin);
    
    int16_t raw_temp = (msb << 8) | lsb;
    return (float)raw_temp / 16.0f;
}
