#ifndef TM1638_H
#define TM1638_H

#include "stm32f4xx_hal.h"

void TM1638_Init(void);
void TM1638_SendCommand(uint8_t cmd);
void TM1638_SendData(uint8_t addr, uint8_t data);
void TM1638_DisplayString(const char *str);
void TM1638_SetLEDs(uint8_t data);
void TM1638_Clear(void);
uint8_t TM1638_ReadButtons(void);
void TM1638_SendDMA(const char *str, uint8_t leds);

#endif
