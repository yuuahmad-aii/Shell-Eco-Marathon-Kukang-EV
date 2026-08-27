#ifndef MOTOR_SERVO_LICHUAN_H
#define MOTOR_SERVO_LICHUAN_H

// --- Encoder Types ---
#define ENCODER_TYPE_INCREMENTAL 1
#define ENCODER_TYPE_AS5600      2

// ==========================================
// USER CONFIGURATION: Select your encoder
// ==========================================
#define ENCODER_USE ENCODER_TYPE_AS5600


// --- AS5600 I2C Configuration ---
#if (ENCODER_USE == ENCODER_TYPE_AS5600)
    #include "stm32g4xx_hal.h"
    // Reference to the I2C handle initialized by CubeMX
    extern I2C_HandleTypeDef hi2c1;
    #define AS5600_I2C_HANDLE &hi2c1
    
    // AS5600 I2C Address (0x36 << 1 = 0x6C)
    #define AS5600_I2C_ADDR 0x6C
    
    // Raw Angle Register (0x0C and 0x0D)
    #define AS5600_REG_RAW_ANGLE 0x0C
    
    // Global variable updated by main loop polling
    extern volatile uint16_t as5600_raw_angle;
    
    // AS5600 has 12-bit resolution -> 4096 counts per revolution
    #define AS5600_CPR 4096
#endif


#endif // MOTOR_SERVO_LICHUAN_H
