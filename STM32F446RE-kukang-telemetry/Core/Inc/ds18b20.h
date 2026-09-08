#ifndef __DS18B20_H
#define __DS18B20_H

#include "main.h"

#define DS18B20_CMD_SEARCHROM     0xF0
#define DS18B20_CMD_READROM       0x33
#define DS18B20_CMD_MATCHROM      0x55
#define DS18B20_CMD_SKIPROM       0xCC
#define DS18B20_CMD_CONVERT       0x44
#define DS18B20_CMD_READSCRATCH   0xBE

typedef struct {
    uint8_t rom_code[8];
    float temperature;
} DS18B20_Device;

void delay_us(uint16_t us);
uint8_t DS18B20_Search(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, DS18B20_Device *devices, uint8_t max_devices);
void DS18B20_StartAll(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
float DS18B20_ReadTemp(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t *rom_code);

#endif /* __DS18B20_H */
