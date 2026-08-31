#include "six_step.h"
#include "main.h"
#include "config.h"
#include <math.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

extern TIM_HandleTypeDef htim1;

// Hybrid control variables
static float current_duty = 0.0f; // 0.0 to 100.0
static uint8_t motor_running = 0;
static uint8_t last_hall_state = 0;

static uint8_t Get_Hall_State(void);

static float electrical_velocity = 0.0f; 
static float time_since_hall = 0.0f;
static float interpolated_angle = 0.0f;
static float time_running = 0.0f;

// Velocity Control Variables
static float target_rpm = 0.0f;
static float ramped_rpm = 0.0f;
static float vel_integral = 0.0f;

// Switchover threshold will be calculated from motor_config.switchover_rpm
static uint8_t svpwm_mode = 0;

static const float hall_angles[8] = {
    0.0f,
    M_PI / 3.0f,        // 1
    M_PI,               // 2
    2.0f * M_PI / 3.0f, // 3
    5.0f * M_PI / 3.0f, // 4
    0.0f,               // 5
    4.0f * M_PI / 3.0f, // 6
    0.0f                // 7
};

// Hardware specific macros for 6-PWM control on TIM1
#define ENABLE_PHASE_U()  htim1.Instance->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC1NE)
#define DISABLE_PHASE_U() htim1.Instance->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC1NE)
#define ENABLE_PHASE_V()  htim1.Instance->CCER |= (TIM_CCER_CC2E | TIM_CCER_CC2NE)
#define DISABLE_PHASE_V() htim1.Instance->CCER &= ~(TIM_CCER_CC2E | TIM_CCER_CC2NE)
#define ENABLE_PHASE_W()  htim1.Instance->CCER |= (TIM_CCER_CC3E | TIM_CCER_CC3NE)
#define DISABLE_PHASE_W() htim1.Instance->CCER &= ~(TIM_CCER_CC3E | TIM_CCER_CC3NE)

void SixStep_Init(void) {
    current_duty = 0.0f;
    motor_running = 0;
    last_hall_state = 0;
    electrical_velocity = 0.0f;
    time_since_hall = 0.0f;
    time_running = 0.0f;
    svpwm_mode = 0;
    
    target_rpm = 0.0f;
    ramped_rpm = 0.0f;
    vel_integral = 0.0f;
    
    SixStep_Stop();
    
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
    
    htim1.Instance->BDTR |= TIM_BDTR_MOE;
}

void SixStep_SetRPM(float setpoint_rpm) {
    if (setpoint_rpm < 0.0f) setpoint_rpm = 0.0f; // Limit to positive for now
    
    target_rpm = setpoint_rpm;
    
    if (target_rpm > 5.0f) {
        if (!motor_running) {
            last_hall_state = Get_Hall_State();
            electrical_velocity = 0.0f;
            time_since_hall = 0.0f;
            time_running = 0.0f;
            ramped_rpm = 0.0f;
            vel_integral = 10.0f; // Pre-load integral slightly to give initial kick
            current_duty = 10.0f;
        }
        motor_running = 1;
    } else {
        SixStep_Stop();
    }
}

void SixStep_Stop(void) {
    motor_running = 0;
    svpwm_mode = 0;
    time_running = 0.0f;
    target_rpm = 0.0f;
    ramped_rpm = 0.0f;
    vel_integral = 0.0f;
    current_duty = 0.0f;
    
    DISABLE_PHASE_U();
    DISABLE_PHASE_V();
    DISABLE_PHASE_W();
    
    htim1.Instance->CCR1 = 0;
    htim1.Instance->CCR2 = 0;
    htim1.Instance->CCR3 = 0;
}

static uint8_t Get_Hall_State(void) {
    uint8_t state = 0;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) state |= 1;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) state |= 2;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET) state |= 4;
    return state;
}

static void SVPWM(float v_alpha, float v_beta, float *t_a, float *t_b, float *t_c) {
    float v_a = v_alpha;
    float v_b = -0.5f * v_alpha + 0.86602540378f * v_beta;
    float v_c = -0.5f * v_alpha - 0.86602540378f * v_beta;

    float v_min = fminf(v_a, fminf(v_b, v_c));
    float v_max = fmaxf(v_a, fmaxf(v_b, v_c));
    float v_com = -0.5f * (v_max + v_min);
    
    *t_a = v_a + v_com + 0.5f;
    *t_b = v_b + v_com + 0.5f;
    *t_c = v_c + v_com + 0.5f;

    if (*t_a > 1.0f) *t_a = 1.0f; 
    if (*t_a < 0.0f) *t_a = 0.0f;
    
    if (*t_b > 1.0f) *t_b = 1.0f; 
    if (*t_b < 0.0f) *t_b = 0.0f;
    
    if (*t_c > 1.0f) *t_c = 1.0f; 
    if (*t_c < 0.0f) *t_c = 0.0f;
}

void SixStep_Update(float dt) {
    if (!motor_running) return;

    // Acceleration Limit
    if (motor_config.accel_rpm_s > 0.0f) {
        if (ramped_rpm < target_rpm) {
            ramped_rpm += motor_config.accel_rpm_s * dt;
            if (ramped_rpm > target_rpm) ramped_rpm = target_rpm;
        } else if (ramped_rpm > target_rpm) {
            ramped_rpm -= motor_config.accel_rpm_s * dt;
            if (ramped_rpm < target_rpm) ramped_rpm = target_rpm;
        }
    } else {
        ramped_rpm = target_rpm;
    }

    time_since_hall += dt;
    time_running += dt;
    uint8_t hall_state = Get_Hall_State();
    
    if (hall_state != last_hall_state) {
        if (last_hall_state != 0 && time_since_hall > 0.0001f) {
            float inst_vel = (M_PI / 3.0f) / time_since_hall;
            // Simple low pass filter
            electrical_velocity = 0.2f * inst_vel + 0.8f * electrical_velocity;
        }
        time_since_hall = 0.0f;
        last_hall_state = hall_state;
    }

    if (time_since_hall > 0.2f) {
        electrical_velocity = 0.0f; // Decay if stalled
    }

    // Calculate expected angle based on Hall sensors
    float hall_offset = motor_config.hall_offset_deg * M_PI / 180.0f;
    float expected_angle = hall_angles[hall_state] + hall_offset + (electrical_velocity * time_since_hall);
    while (expected_angle > 2.0f * M_PI) expected_angle -= 2.0f * M_PI;
    while (expected_angle < 0.0f) expected_angle += 2.0f * M_PI;

    if (svpwm_mode) {
        // Integrate angle smoothly
        interpolated_angle += electrical_velocity * dt;
        
        // Phase Locked Loop (PLL) to gently pull interpolated_angle to expected_angle
        float err = expected_angle - interpolated_angle;
        while (err > M_PI) err -= 2.0f * M_PI;
        while (err < -M_PI) err += 2.0f * M_PI;
        
        interpolated_angle += err * 0.05f; // Pull 5% of error per cycle
        
        while (interpolated_angle > 2.0f * M_PI) interpolated_angle -= 2.0f * M_PI;
        while (interpolated_angle < 0.0f) interpolated_angle += 2.0f * M_PI;
    } else {
        // In 6-step, snap exactly to expected angle so we are ready for a smooth switchover
        interpolated_angle = expected_angle;
    }

    // Calculate switchover velocity threshold dynamically
    float switchover_velocity = 50.0f; // Default safe value
    if (motor_config.pole_pairs > 0) {
        switchover_velocity = motor_config.switchover_rpm * motor_config.pole_pairs * (2.0f * M_PI) / 60.0f;
    }

    // Hysteresis for mode switching
    uint8_t next_svpwm_mode = svpwm_mode;
    if (time_running > motor_config.switchover_delay && electrical_velocity > switchover_velocity && current_duty > 5.0f) {
        next_svpwm_mode = 1;
    } else if (electrical_velocity < (switchover_velocity - 15.0f)) {
        next_svpwm_mode = 0;
    }
    
    // Bumpless Transfer: Adjust integral state when switching modes
    if (next_svpwm_mode == 1 && svpwm_mode == 0) {
        // 6-step to SVPWM: Need ~15.4% more duty cycle for equivalent voltage
        vel_integral *= 1.1547f; 
    } else if (next_svpwm_mode == 0 && svpwm_mode == 1) {
        // SVPWM to 6-step
        vel_integral *= 0.8660f; 
    }
    svpwm_mode = next_svpwm_mode;

    // --- PI Velocity Controller ---
    float current_rpm = 0.0f;
    if (motor_config.pole_pairs > 0) {
        current_rpm = electrical_velocity * 60.0f / (2.0f * M_PI * motor_config.pole_pairs);
    }

    float rpm_error = ramped_rpm - current_rpm;
    vel_integral += rpm_error * motor_config.vel_ki * dt;
    
    // Anti-windup
    if (vel_integral > 100.0f) vel_integral = 100.0f;
    if (vel_integral < 0.0f) vel_integral = 0.0f;

    float pi_out = (rpm_error * motor_config.vel_kp) + vel_integral;
    if (pi_out > 100.0f) pi_out = 100.0f;
    if (pi_out < 0.0f) pi_out = 0.0f;
    
    current_duty = pi_out;
    // ------------------------------

    uint32_t arr = htim1.Instance->ARR;
    uint32_t ccr_val = (uint32_t)((current_duty / 100.0f) * arr);

    if (svpwm_mode) {
        ENABLE_PHASE_U();
        ENABLE_PHASE_V();
        ENABLE_PHASE_W();

        float v_mag = current_duty / 100.0f; 
        float v_alpha = v_mag * cosf(interpolated_angle);
        float v_beta  = v_mag * sinf(interpolated_angle);

        float ta, tb, tc;
        SVPWM(v_alpha, v_beta, &ta, &tb, &tc);

        htim1.Instance->CCR1 = (uint32_t)(ta * arr);
        htim1.Instance->CCR2 = (uint32_t)(tb * arr);
        htim1.Instance->CCR3 = (uint32_t)(tc * arr);
    } else {
        // 6-step block commutation
        switch (hall_state) {
            case 5:
                DISABLE_PHASE_W();
                htim1.Instance->CCR1 = ccr_val;
                htim1.Instance->CCR2 = 0;
                ENABLE_PHASE_U();
                ENABLE_PHASE_V();
                break;
            case 1:
                DISABLE_PHASE_V();
                htim1.Instance->CCR1 = ccr_val;
                htim1.Instance->CCR3 = 0;
                ENABLE_PHASE_U();
                ENABLE_PHASE_W();
                break;
            case 3:
                DISABLE_PHASE_U();
                htim1.Instance->CCR2 = ccr_val;
                htim1.Instance->CCR3 = 0;
                ENABLE_PHASE_V();
                ENABLE_PHASE_W();
                break;
            case 2:
                DISABLE_PHASE_W();
                htim1.Instance->CCR1 = 0;
                htim1.Instance->CCR2 = ccr_val;
                ENABLE_PHASE_U();
                ENABLE_PHASE_V();
                break;
            case 6:
                DISABLE_PHASE_V();
                htim1.Instance->CCR1 = 0;
                htim1.Instance->CCR3 = ccr_val;
                ENABLE_PHASE_U();
                ENABLE_PHASE_W();
                break;
            case 4:
                DISABLE_PHASE_U();
                htim1.Instance->CCR2 = 0;
                htim1.Instance->CCR3 = ccr_val;
                ENABLE_PHASE_V();
                ENABLE_PHASE_W();
                break;
            default:
                SixStep_Stop();
                motor_running = 1; 
                break;
        }
    }
}

extern void cdc_printf(const char *format, ...);
extern void Telemetry_SendBinary(float pos, float vel, float vq, float target, float ia, float ib, float ic, uint8_t mode);

void SixStep_PrintVerbose(void) {
    float rpm = 0.0f;
    if (motor_config.pole_pairs > 0) {
        rpm = electrical_velocity * 60.0f / (2.0f * M_PI * motor_config.pole_pairs);
    }
    
    float duty_u = 0.0f, duty_v = 0.0f, duty_w = 0.0f;
    uint32_t arr = htim1.Instance->ARR;
    if (arr > 0) {
        duty_u = (htim1.Instance->CCR1 / (float)arr) * 100.0f;
        duty_v = (htim1.Instance->CCR2 / (float)arr) * 100.0f;
        duty_w = (htim1.Instance->CCR3 / (float)arr) * 100.0f;
    }
    
    uint8_t mode = motor_running ? (svpwm_mode ? 2 : 1) : 0;
    
    // Send binary to GUI
    // We send: pos=interpolated_angle, vel=rpm, vq=current_duty, target=current_duty, 
    // ia=duty_u, ib=duty_v, ic=duty_w, mode=mode
    Telemetry_SendBinary(interpolated_angle, rpm, current_duty, current_duty, duty_u, duty_v, duty_w, mode);
}
