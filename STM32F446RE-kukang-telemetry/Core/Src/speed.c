#include "speed.h"
#include "config.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern TIM_HandleTypeDef htim2; // Right wheel
extern TIM_HandleTypeDef htim5; // Left wheel

float speed_left_kmh = 0.0f;
float speed_right_kmh = 0.0f;
float rpm_tim2ch1 = 0.0f;
float rpm_tim5ch2 = 0.0f;

static uint32_t last_ic_left = 0;
static uint32_t last_ic_right = 0;
static uint32_t last_tick_left = 0;
static uint32_t last_tick_right = 0;

void Speed_Init(void) {
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_2);
}

static float calculate_rpm(uint32_t delta_t) {
    if (delta_t == 0 || current_config.pulses_per_rev == 0) return 0.0f;
    
    // Timer clock is APB1 Timer Clock: PCLK1 * 2 = 84MHz
    uint32_t timer_clock = HAL_RCC_GetPCLK1Freq() * 2;
    float freq_hz = (float)timer_clock / (float)delta_t;
    return (freq_hz * 60.0f / current_config.pulses_per_rev);
}

static float calculate_speed_from_rpm(float rpm) {
    // Wheel circumference in mm = PI * diameter
    // Speed km/h = RPM * circumference(mm) * 60 / 1,000,000
    float circumference = M_PI * current_config.wheel_diameter_mm;
    return (rpm * circumference * 60.0f / 1000000.0f);
}

// Call this in while(1) to reset speed and RPM to 0 if car stopped
void Speed_UpdateTimeout(void) {
    uint32_t current_tick = HAL_GetTick();
    
    // Timeout of 2000ms = stopped
    if (current_tick - last_tick_left > 2000) {
        speed_left_kmh = 0.0f;
        rpm_tim5ch2 = 0.0f;
    }
    if (current_tick - last_tick_right > 2000) {
        speed_right_kmh = 0.0f;
        rpm_tim2ch1 = 0.0f;
    }
}

// Call this inside HAL_TIM_IC_CaptureCallback
void Speed_IC_Callback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        uint32_t ic_val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        uint32_t delta = ic_val - last_ic_right; // 32-bit wrap around is handled automatically
        
        if (delta > 0 && last_ic_right != 0) {
            rpm_tim2ch1 = calculate_rpm(delta);
            speed_right_kmh = calculate_speed_from_rpm(rpm_tim2ch1);
        }
        
        last_ic_right = ic_val;
        last_tick_right = HAL_GetTick();
    }
    else if (htim->Instance == TIM5 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
        uint32_t ic_val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
        uint32_t delta = ic_val - last_ic_left;
        
        if (delta > 0 && last_ic_left != 0) {
            rpm_tim5ch2 = calculate_rpm(delta);
            speed_left_kmh = calculate_speed_from_rpm(rpm_tim5ch2);
        }
        
        last_ic_left = ic_val;
        last_tick_left = HAL_GetTick();
    }
}
