#ifndef HUB_MOTOR_EBIKE_H
#define HUB_MOTOR_EBIKE_H

// --- Control Algorithms ---
#define CONTROL_FOC      1
#define CONTROL_SIX_STEP 2

// ==========================================
// USER CONFIGURATION: Select your control algorithm
// ==========================================
#define CONTROL_ALGORITHM CONTROL_SIX_STEP

// --- Encoder Types ---
#define ENCODER_TYPE_NONE        0
#define ENCODER_TYPE_INCREMENTAL 1
#define ENCODER_TYPE_AS5600      2
#define ENCODER_TYPE_HALL        3

// ==========================================
// USER CONFIGURATION: Select your encoder
// ==========================================
// Set to NONE for open-loop testing. Change to HALL when ready for closed-loop.
#define ENCODER_USE ENCODER_TYPE_NONE

// --- Hall Sensor Configuration ---
#if (ENCODER_USE == ENCODER_TYPE_HALL)
    #include "stm32g4xx_hal.h"
    
    // Hub motors commonly use 3 Hall sensors to estimate rotor position.
    // In this project, TIM2 is configured as the Hall Sensor Interface.
    extern TIM_HandleTypeDef htim2;
    #define HALL_TIM_HANDLE &htim2
    
    // Usually a hub motor has multiple pole pairs (e.g. 15 or 23).
    // Make sure to configure this properly in motor_config.
#endif

#endif // HUB_MOTOR_EBIKE_H
