#include "open_loop.h"
#include "config.h"
#include "main.h"
#include <math.h>

open_loop_state_t open_loop;

extern TIM_HandleTypeDef htim1;

#define PI 3.14159265359f
#define SQRT3_2 0.86602540378f

// Fast CORDIC Sin/Cos calculation
// Replaces math.h sinf/cosf with hardware acceleration
static inline void CORDIC_Fast_SinCos(float angle_rad, float *s, float *c) {
    // Wrap to [-PI, PI)
    while (angle_rad >= PI) angle_rad -= 2.0f * PI;
    while (angle_rad < -PI) angle_rad += 2.0f * PI;
    
    // Convert to Q1.31 format
    int32_t q31_angle = (int32_t)((angle_rad / PI) * 2147483648.0f);
    
    // Write argument (triggers calculation)
    CORDIC->WDATA = q31_angle;
    
    // Read results: Cosine first, then Sine
    int32_t q31_cos = (int32_t)CORDIC->RDATA;
    int32_t q31_sin = (int32_t)CORDIC->RDATA;
    
    // Convert back to float
    *c = (float)q31_cos / 2147483648.0f;
    *s = (float)q31_sin / 2147483648.0f;
}

// Calculate modulo for float to stay within [0, 2PI]
static float _wrap_2pi(float angle) {
    while (angle < 0.0f) angle += 2.0f * PI;
    while (angle > 2.0f * PI) angle -= 2.0f * PI;
    return angle;
}

void OpenLoop_Init(void) {
    // Configure CORDIC for Fast Sine/Cosine
    // FUNC = 0 (Cosine/Sine), PRECISION = 5 (24-bit, 6 cycles)
    // SCALE = 0, IEN = 0, NRES = 1 (2 results), NARGS = 0 (1 arg)
    // Note: ensure __HAL_RCC_CORDIC_CLK_ENABLE() is called if not done by HAL_Init
    __HAL_RCC_CORDIC_CLK_ENABLE();
    CORDIC->CSR = (0x00 << CORDIC_CSR_FUNC_Pos) |
                  (5 << CORDIC_CSR_PRECISION_Pos) |
                  (1 << CORDIC_CSR_NRES_Pos) | 
                  (0 << CORDIC_CSR_NARGS_Pos);

    open_loop.electrical_angle = 0.0f;
    open_loop.mechanical_angle = 0.0f;
    open_loop.shaft_velocity = 0.0f;
    open_loop.target_velocity = 0.0f;
    open_loop.ramped_target_velocity = 0.0f;
    
    open_loop.v_q = 0.0f;
    open_loop.v_d = 0.0f;
    open_loop.enabled = 0;
    
    disable_mosfet();
}

void OpenLoop_SetTarget(float velocity_rad_s) {
    open_loop.target_velocity = velocity_rad_s;
    if (fabsf(velocity_rad_s) > 0.1f) {
        if (!open_loop.enabled) {
            enable_mosfet();
        }
    } else {
        disable_mosfet();
    }
}

void OpenLoop_Update(float dt) {
    if (dt <= 0.0f) return;

    if (!open_loop.enabled) {
        open_loop.ramped_target_velocity = 0.0f;
        open_loop.shaft_velocity = 0.0f;
        open_loop.v_q = 0.0f;
        open_loop.v_d = 0.0f;
        return;
    }

    // Apply acceleration limit
    if (motor_config.accel_limit > 0.0f) {
        float max_dv = motor_config.accel_limit * dt;
        if (open_loop.target_velocity > open_loop.ramped_target_velocity + max_dv) {
            open_loop.ramped_target_velocity += max_dv;
        } else if (open_loop.target_velocity < open_loop.ramped_target_velocity - max_dv) {
            open_loop.ramped_target_velocity -= max_dv;
        } else {
            open_loop.ramped_target_velocity = open_loop.target_velocity;
        }
    } else {
        open_loop.ramped_target_velocity = open_loop.target_velocity;
    }
    
    open_loop.shaft_velocity = open_loop.ramped_target_velocity;

    // Advance mechanical and electrical angles
    open_loop.mechanical_angle += open_loop.shaft_velocity * dt;
    open_loop.mechanical_angle = _wrap_2pi(open_loop.mechanical_angle);

    open_loop.electrical_angle = _wrap_2pi(open_loop.mechanical_angle * motor_config.pole_pairs);

    // Apply Open Loop Voltage (on Q-axis)
    // You can adjust v_d and v_q depending on desired open loop V/F characteristics
    // We just apply a constant voltage in the direction of motion
    if (open_loop.shaft_velocity > 0.01f) {
        open_loop.v_q = motor_config.open_loop_voltage;
    } else if (open_loop.shaft_velocity < -0.01f) {
        open_loop.v_q = -motor_config.open_loop_voltage;
    } else {
        open_loop.v_q = 0.0f;
    }
    open_loop.v_d = 0.0f;

    // Inverse Park Transform
    float s, c;
    CORDIC_Fast_SinCos(open_loop.electrical_angle, &s, &c);

    float v_alpha = open_loop.v_d * c - open_loop.v_q * s;
    float v_beta  = open_loop.v_d * s + open_loop.v_q * c;

    // Inverse Clarke Transform
    open_loop.u_a = v_alpha;
    open_loop.u_b = -0.5f * v_alpha + SQRT3_2 * v_beta;
    open_loop.u_c = -0.5f * v_alpha - SQRT3_2 * v_beta;

    // Space Vector PWM (SVPWM) Midpoint Clamping
    float v_min = open_loop.u_a;
    if (open_loop.u_b < v_min) v_min = open_loop.u_b;
    if (open_loop.u_c < v_min) v_min = open_loop.u_c;

    float v_max = open_loop.u_a;
    if (open_loop.u_b > v_max) v_max = open_loop.u_b;
    if (open_loop.u_c > v_max) v_max = open_loop.u_c;

    float v_neutral = 0.5f * (v_min + v_max);

    open_loop.u_a -= v_neutral;
    open_loop.u_b -= v_neutral;
    open_loop.u_c -= v_neutral;

    // Scale to duty cycle (0.0 to 1.0)
    float v_bus = motor_config.dc_bus_voltage;
    if (v_bus < 1.0f) v_bus = 1.0f; 

    open_loop.duty_a = (open_loop.u_a / v_bus) + 0.5f;
    open_loop.duty_b = (open_loop.u_b / v_bus) + 0.5f;
    open_loop.duty_c = (open_loop.u_c / v_bus) + 0.5f;

    // Constrain duty cycles
    if (open_loop.duty_a < 0.0f) open_loop.duty_a = 0.0f;
    if (open_loop.duty_a > 1.0f) open_loop.duty_a = 1.0f;
    if (open_loop.duty_b < 0.0f) open_loop.duty_b = 0.0f;
    if (open_loop.duty_b > 1.0f) open_loop.duty_b = 1.0f;
    if (open_loop.duty_c < 0.0f) open_loop.duty_c = 0.0f;
    if (open_loop.duty_c > 1.0f) open_loop.duty_c = 1.0f;

    // Apply to TIM1 CCR (CounterMode = UP, so ARR is the period)
    uint32_t period = htim1.Instance->ARR + 1; 
    htim1.Instance->CCR1 = (uint32_t)(open_loop.duty_a * period);
    htim1.Instance->CCR2 = (uint32_t)(open_loop.duty_b * period);
    htim1.Instance->CCR3 = (uint32_t)(open_loop.duty_c * period);
}

void enable_mosfet(void) {
    // For IR2101, Timer automatically generates complementary outputs (CHx and CHxN)
    // Enable Main Output Enable (MOE) to enable both high and low sides
    htim1.Instance->BDTR |= TIM_BDTR_MOE;
    open_loop.enabled = 1;
}

void disable_mosfet(void) {
    open_loop.enabled = 0;
    // Set 0 duty cycle
    htim1.Instance->CCR1 = 0;
    htim1.Instance->CCR2 = 0;
    htim1.Instance->CCR3 = 0;
    
    // Disable Main Output Enable (MOE) to float both high and low sides
    htim1.Instance->BDTR &= ~TIM_BDTR_MOE;
}
