#ifndef __SPEED_H
#define __SPEED_H

#include "main.h"

// Speed and RPM variables
extern float speed_left_kmh;
extern float speed_right_kmh;
extern float rpm_tim2ch1; // TIM2 CH1 (Right Wheel)
extern float rpm_tim5ch2; // TIM5 CH2 (Left Wheel)

void Speed_Init(void);
void Speed_UpdateTimeout(void);
void Speed_IC_Callback(TIM_HandleTypeDef *htim);

#endif // __SPEED_H
