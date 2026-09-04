#ifndef __SPEED_H
#define __SPEED_H

#include "main.h"

// Speed variables
extern float speed_left_kmh;
extern float speed_right_kmh;

void Speed_Init(void);
void Speed_UpdateTimeout(void);
void Speed_IC_Callback(TIM_HandleTypeDef *htim);

#endif // __SPEED_H
