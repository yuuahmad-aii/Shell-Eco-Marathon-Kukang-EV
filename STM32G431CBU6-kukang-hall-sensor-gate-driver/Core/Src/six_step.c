#include "six_step.h"
#include "main.h"
#include "config.h"
#include <math.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

// Hybrid control variables
static float current_duty = 0.0f; // 0.0 to 100.0
static uint8_t motor_running = 0;
static volatile uint8_t current_hall_state = 0;
static volatile uint8_t last_hall_state = 0;
static volatile uint32_t last_capture_ticks = 0;
static volatile uint32_t last_hall_tick = 0;
static volatile uint8_t hall_edge_count = 0;
static float tim2_tick_freq = 0.0f;

static uint8_t Get_Hall_State(void);
static int8_t Get_Hall_Direction(uint8_t current, uint8_t previous);
static void SixStep_ApplyCommutation(uint8_t hall_state, uint32_t ccr_val);

static volatile float electrical_velocity = 0.0f; 
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

static void SixStep_ApplyCommutation(uint8_t hall_state, uint32_t ccr_val) {
    uint8_t comm_state = hall_state;
    if (current_duty < 0.0f) {
        // Reverse commutation by shifting 180 electrical degrees (3 states)
        switch (comm_state) {
            case 5: comm_state = 2; break;
            case 1: comm_state = 6; break;
            case 3: comm_state = 4; break;
            case 2: comm_state = 5; break;
            case 6: comm_state = 1; break;
            case 4: comm_state = 3; break;
            default: break;
        }
    }

    switch (comm_state) {
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
            DISABLE_PHASE_U();
            DISABLE_PHASE_V();
            DISABLE_PHASE_W();
            break;
    }
}

/**
 * @brief HAL Input Capture Callback for TIM2 Hall Sensor Interface
 *        Automatically triggered on any Hall edge (XOR of PA0, PA1, PA2).
 *        The hardware latches the counter into CCR1 and resets CNT to 0,
 *        giving absolute nanosecond-precision delta-t measurement.
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        uint32_t capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        uint8_t hall_state = Get_Hall_State();
        uint32_t now_tick = HAL_GetTick();

        if (hall_state >= 1 && hall_state <= 6) {
            if (last_hall_state != 0 && last_hall_state != hall_state) {
                int8_t dir = Get_Hall_Direction(hall_state, last_hall_state);

                // Ignore invalid sequence jumps (dir == 0) caused by noise
                if (dir != 0) {
                    hall_edge_count++;

                    // Minimum plausible tick count for 7 pole-pair motor:
                    // At 6,000 RPM mechanical (42,000 electrical RPM), 60 deg is 238 us = ~34,000 ticks at 144MHz.
                    // Reject any capture < 15,000 ticks (~104 us, >13,700 RPM) as electrical switching glitch.
                    // Also require at least 2 edges so the first edge (from standstill/stall) is not used.
                    if (hall_edge_count >= 2 && capture >= 15000 && tim2_tick_freq > 0.0f) {
                        float dt_hall = (float)capture / tim2_tick_freq;
                        float inst_vel = (float)dir * (M_PI / 3.0f) / dt_hall;

                        // Clamp maximum plausible velocity:
                        // 2500 rad/s electrical corresponds to ~3400 RPM mechanical for 7 pole pairs.
                        if (fabsf(inst_vel) <= 2500.0f) {
                            if (fabsf(electrical_velocity) < 1.0f) {
                                electrical_velocity = inst_vel;
                            } else {
                                // Limit maximum change per Hall step to physical rotor acceleration limits
                                float max_step_change = fabsf(electrical_velocity) * 0.7f + 50.0f;
                                float diff = inst_vel - electrical_velocity;
                                if (diff > max_step_change) inst_vel = electrical_velocity + max_step_change;
                                if (diff < -max_step_change) inst_vel = electrical_velocity - max_step_change;

                                // Filter out mechanical sensor placement asymmetry smoothly
                                electrical_velocity = 0.2f * inst_vel + 0.8f * electrical_velocity;
                            }
                            last_capture_ticks = capture;
                        }
                    }
                }
            }
            last_hall_state = hall_state;
            current_hall_state = hall_state;
            last_hall_tick = now_tick;

            // Zero-latency instant phase commutation on edge in 6-step mode
            if (motor_running && !svpwm_mode) {
                uint32_t arr = htim1.Instance->ARR;
                uint32_t ccr_val = (uint32_t)((fabsf(current_duty) / 100.0f) * arr);
                SixStep_ApplyCommutation(hall_state, ccr_val);
            }
        }
    }
}

void SixStep_Init(void) {
    current_duty = 0.0f;
    motor_running = 0;
    current_hall_state = 0;
    last_hall_state = 0;
    last_capture_ticks = 0;
    last_hall_tick = 0;
    hall_edge_count = 0;
    electrical_velocity = 0.0f;
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

    // Calculate TIM2 clock frequency dynamically
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    RCC_ClkInitTypeDef clkConfig;
    uint32_t flatency;
    HAL_RCC_GetClockConfig(&clkConfig, &flatency);
    uint32_t tim2_clock = (clkConfig.APB1CLKDivider == RCC_HCLK_DIV1) ? pclk1 : (pclk1 * 2);
    tim2_tick_freq = (float)tim2_clock / (float)(htim2.Init.Prescaler + 1);
    if (tim2_tick_freq <= 0.0f) {
        tim2_tick_freq = (float)SystemCoreClock;
    }

    // Ensure 256-clock digital filter on TIM2 Channel 1 input in hardware
    TIM2->CCMR1 = (TIM2->CCMR1 & ~TIM_CCMR1_IC1F) | (0x0F << TIM_CCMR1_IC1F_Pos);

    // Start TIM2 Hall Sensor Interface in Interrupt mode
    HAL_TIMEx_HallSensor_Start_IT(&htim2);

    current_hall_state = Get_Hall_State();
    last_hall_state = current_hall_state;
    last_hall_tick = HAL_GetTick();
}

void SixStep_SetRPM(float setpoint_rpm) {
    target_rpm = setpoint_rpm;
    
    if (fabsf(target_rpm) > 5.0f) {
        if (!motor_running) {
            // --- BOOTSTRAP PRE-CHARGE FOR IR2110 ---
            // Set 0% duty (HIN=0, LIN=1) and enable all phases
            htim1.Instance->CCR1 = 0;
            htim1.Instance->CCR2 = 0;
            htim1.Instance->CCR3 = 0;
            ENABLE_PHASE_U();
            ENABLE_PHASE_V();
            ENABLE_PHASE_W();
            
            // Wait 5ms to fully charge bootstrap capacitors
            HAL_Delay(5);
            
            // --- SINGLE ALIGNMENT (STARTUP) ---
            if (motor_config.startup_align_ms > 0) {
                // Apply a static vector: Phase U high, V low, W float
                uint32_t align_ccr = (uint32_t)((motor_config.startup_align_duty / 100.0f) * htim1.Instance->ARR);
                htim1.Instance->CCR1 = align_ccr;
                htim1.Instance->CCR2 = 0;
                htim1.Instance->CCR3 = 0;
                
                ENABLE_PHASE_U();
                ENABLE_PHASE_V();
                DISABLE_PHASE_W();
                
                HAL_Delay(motor_config.startup_align_ms);
            }
            
            // Disable phases before starting normal commutation
            DISABLE_PHASE_U();
            DISABLE_PHASE_V();
            DISABLE_PHASE_W();
            // ---------------------------------------

            current_hall_state = Get_Hall_State();
            last_hall_state = current_hall_state;
            last_hall_tick = HAL_GetTick();
            last_capture_ticks = 0;
            hall_edge_count = 0;
            electrical_velocity = 0.0f;
            time_running = 0.0f;
            ramped_rpm = 0.0f;
            vel_integral = (target_rpm > 0.0f) ? 10.0f : -10.0f; // Pre-load integral slightly to give initial kick
            current_duty = (target_rpm > 0.0f) ? 10.0f : -10.0f;

            // Apply initial commutation step based on current Hall state
            uint32_t arr = htim1.Instance->ARR;
            uint32_t ccr_val = (uint32_t)((fabsf(current_duty) / 100.0f) * arr);
            SixStep_ApplyCommutation(current_hall_state, ccr_val);
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
    electrical_velocity = 0.0f;
    last_capture_ticks = 0;
    hall_edge_count = 0;
    
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

static int8_t Get_Hall_Direction(uint8_t current, uint8_t previous) {
    if (current == previous) return 0;
    switch (previous) {
        case 5: return (current == 1) ? 1 : ((current == 4) ? -1 : 0);
        case 1: return (current == 3) ? 1 : ((current == 5) ? -1 : 0);
        case 3: return (current == 2) ? 1 : ((current == 1) ? -1 : 0);
        case 2: return (current == 6) ? 1 : ((current == 3) ? -1 : 0);
        case 6: return (current == 4) ? 1 : ((current == 2) ? -1 : 0);
        case 4: return (current == 5) ? 1 : ((current == 6) ? -1 : 0);
        default: return 0;
    }
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

    if (*t_a > 0.95f) *t_a = 0.95f; // Limit to 95% for bootstrap recharge
    if (*t_a < 0.0f) *t_a = 0.0f;
    
    if (*t_b > 0.95f) *t_b = 0.95f; 
    if (*t_b < 0.0f) *t_b = 0.0f;
    
    if (*t_c > 0.95f) *t_c = 0.95f; 
    if (*t_c < 0.0f) *t_c = 0.0f;
}

void SixStep_Update(float dt) {
    if (!motor_running) {
        electrical_velocity = 0.0f;
        interpolated_angle = 0.0f;
        current_duty = 0.0f;
        vel_integral = 0.0f;
        return;
    }

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

    time_running += dt;

    // Elapsed time since last Hall edge obtained directly from TIM2 counter (absolute precision, zero polling jitter)
    float time_since_hall = 0.0f;
    if (tim2_tick_freq > 0.0f) {
        time_since_hall = (float)htim2.Instance->CNT / tim2_tick_freq;
    }

    // Timeout check: if no edge for > 200ms, the motor has stalled or stopped
    if (HAL_GetTick() - last_hall_tick > 200) {
        electrical_velocity = 0.0f;
        hall_edge_count = 0;
        time_since_hall = 0.2f;
    } else if (last_capture_ticks > 0 && htim2.Instance->CNT > (last_capture_ticks * 2)) {
        // Fast deceleration tracking: current step is taking longer than 2x previous step
        float max_possible_vel = (M_PI / 3.0f) / time_since_hall;
        if (electrical_velocity > max_possible_vel) {
            electrical_velocity = max_possible_vel;
        } else if (electrical_velocity < -max_possible_vel) {
            electrical_velocity = -max_possible_vel;
        }
    }

    uint8_t hall_state = current_hall_state;

    // Calculate expected angle based on Hall sensors
    float hall_offset = motor_config.hall_offset_deg * M_PI / 180.0f;
    float expected_angle = hall_angles[hall_state] + hall_offset;
    
    // hall_angles stores the forward entry edge. For reverse, the entry edge is 60 degrees ahead.
    if (ramped_rpm < 0.0f) {
        expected_angle += (M_PI / 3.0f);
    }
    
    expected_angle += (electrical_velocity * time_since_hall);
    
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

    // --- Current RPM Calculation ---
    float current_rpm = 0.0f;
    if (motor_config.pole_pairs > 0) {
        current_rpm = electrical_velocity * 60.0f / (2.0f * M_PI * motor_config.pole_pairs);
    }

    // Mode switching: ONLY switch to SVPWM if commanded target is actually >= switchover_rpm
    uint8_t next_svpwm_mode = svpwm_mode;
    float switchover_rpm = motor_config.switchover_rpm;
    if (switchover_rpm > 10.0f && fabsf(ramped_rpm) >= switchover_rpm) {
        float min_delay = (motor_config.switchover_delay > 0.2f) ? motor_config.switchover_delay : 0.2f;
        if (time_running > min_delay && fabsf(current_rpm) >= (switchover_rpm - 15.0f) && fabsf(current_duty) > 5.0f) {
            next_svpwm_mode = 1;
        } else if (fabsf(current_rpm) < (switchover_rpm - 30.0f)) {
            next_svpwm_mode = 0;
        }
    } else {
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

    // --- PI Velocity Controller with Direction-Constrained Limits ---
    // Prevents severe plugging/reverse voltage spikes during forward drive
    float rpm_error = ramped_rpm - current_rpm;
    vel_integral += rpm_error * motor_config.vel_ki * dt;
    
    float max_duty = 95.0f; // Limit max duty for IR2110 bootstrap recharge
    
    if (ramped_rpm >= 0.0f) {
        // Forward drive: duty must NEVER go negative into reverse plugging
        if (vel_integral > max_duty) vel_integral = max_duty;
        if (vel_integral < 0.0f) vel_integral = 0.0f;

        float pi_out = (rpm_error * motor_config.vel_kp) + vel_integral;
        if (pi_out > max_duty) pi_out = max_duty;
        if (pi_out < 0.0f) pi_out = 0.0f;
        current_duty = pi_out;
    } else {
        // Reverse drive: duty must NEVER go positive
        if (vel_integral < -max_duty) vel_integral = -max_duty;
        if (vel_integral > 0.0f) vel_integral = 0.0f;

        float pi_out = (rpm_error * motor_config.vel_kp) + vel_integral;
        if (pi_out < -max_duty) pi_out = -max_duty;
        if (pi_out > 0.0f) pi_out = 0.0f;
        current_duty = pi_out;
    }

    uint32_t arr = htim1.Instance->ARR;
    uint32_t ccr_val = (uint32_t)((fabsf(current_duty) / 100.0f) * arr);

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
        SixStep_ApplyCommutation(hall_state, ccr_val);
    }
}

extern void cdc_printf(const char *format, ...);
extern void Telemetry_SendBinary(float pos, float vel, float vq, float target, float ia, float ib, float ic, float vbus, uint8_t mode);

void SixStep_PrintVerbose(void) {
    float rpm = 0.0f;
    if (motor_config.pole_pairs > 0) {
        rpm = electrical_velocity * 60.0f / (2.0f * M_PI * motor_config.pole_pairs);
    }
    
    uint8_t mode = motor_running ? (svpwm_mode ? 2 : 1) : 0;
    
    // Send binary to GUI
    // We send: pos=interpolated_angle, vel=rpm, vq=actual_iq, target=target_iq, 
    // ia=current_u, ib=current_v, ic=current_w, vbus=Get_DC_Bus_Voltage(), mode=mode
    extern float Get_Current_U(void);
    extern float Get_Current_V(void);
    extern float Get_Current_W(void);
    extern float Get_Current_Iq(void);
    extern float Get_DC_Bus_Voltage(void);
    
    // For now, target_iq is mapped from current_duty until the PI controller is implemented
    float target_iq = current_duty; 
    
    Telemetry_SendBinary(interpolated_angle, rpm, Get_Current_Iq(), target_iq, Get_Current_U(), Get_Current_V(), Get_Current_W(), Get_DC_Bus_Voltage(), mode);
}

void SixStep_PrintDebug(void) {
    uint8_t h_state = Get_Hall_State();
    uint8_t h1 = (h_state & 1) ? 1 : 0;
    uint8_t h2 = (h_state & 2) ? 1 : 0;
    uint8_t h3 = (h_state & 4) ? 1 : 0;
    
    cdc_printf("Hall Sensors: H1(PA0)=%d H2(PA1)=%d H3(PA2)=%d [State=%d]\r\n", h1, h2, h3, h_state);
    
    uint32_t ccer = htim1.Instance->CCER;
    uint8_t uh = (ccer & TIM_CCER_CC1E) ? 1 : 0;
    uint8_t ul = (ccer & TIM_CCER_CC1NE) ? 1 : 0;
    uint8_t vh = (ccer & TIM_CCER_CC2E) ? 1 : 0;
    uint8_t vl = (ccer & TIM_CCER_CC2NE) ? 1 : 0;
    uint8_t wh = (ccer & TIM_CCER_CC3E) ? 1 : 0;
    uint8_t wl = (ccer & TIM_CCER_CC3NE) ? 1 : 0;
    
    cdc_printf("MOSFET Gates: UH=%d UL=%d | VH=%d VL=%d | WH=%d WL=%d\r\n", uh, ul, vh, vl, wh, wl);
    
    extern float Get_Current_U(void);
    extern float Get_Current_V(void);
    extern float Get_Current_W(void);
    extern float Get_DC_Bus_Voltage(void);
    cdc_printf("Current (ACS712): U=%.2f A | V=%.2f A | W=%.2f A | Vbus: %.2f V\r\n", Get_Current_U(), Get_Current_V(), Get_Current_W(), Get_DC_Bus_Voltage());
}

float SixStep_GetElectricalAngle(void) {
    return interpolated_angle;
}

uint8_t SixStep_GetHallState(void) {
    return Get_Hall_State();
}

// ==============================================================================
// AUTOMATIC HALL SENSOR OFFSET CALIBRATION
// ==============================================================================
// Sweeps the electrical voltage vector quasi-statically in forward and reverse,
// recording the exact electrical angle at every Hall sensor state transition.
// Uses circular vector averaging to cancel rotor magnetic/friction lag and compute
// the exact hall_offset_deg for optimal MTPA (Maximum Torque Per Ampere) FOC/SVPWM.
// ==============================================================================
void SixStep_CalibrateHall(float cal_voltage) {
    extern float Get_DC_Bus_Voltage(void);
    extern float Get_Current_U(void);
    extern float Get_Current_V(void);
    extern float Get_Current_W(void);
    extern volatile uint8_t can_motor_active;
    extern volatile uint32_t last_can_cmd_tick;

    // 1. Safety stops & variable backup
    SixStep_Stop();
    can_motor_active = 0;
    last_can_cmd_tick = HAL_GetTick();

    uint32_t old_verbose = motor_config.verbose_output;
    motor_config.verbose_output = 0; // Disable binary telemetry spam during calibration

    float vbus = Get_DC_Bus_Voltage();
    if (vbus < 8.0f) {
        vbus = (motor_config.dc_bus_voltage > 8.0f) ? motor_config.dc_bus_voltage : 24.0f;
    }

    if (cal_voltage <= 0.5f) {
        cal_voltage = (motor_config.open_loop_voltage >= 1.0f) ? motor_config.open_loop_voltage : 2.5f;
    }

    float duty = cal_voltage / vbus;
    if (duty > 0.25f) duty = 0.25f; // Hard safety clamp to max 25% duty
    if (duty < 0.03f) duty = 0.03f; // Min 3% duty

    cdc_printf("\r\n==================================================\r\n");
    cdc_printf("           HALL SENSOR CALIBRATION START          \r\n");
    cdc_printf("==================================================\r\n");
    cdc_printf("DC Bus Voltage : %.2f V\r\n", vbus);
    cdc_printf("Cal Voltage    : %.2f V (Duty: %.1f%%)\r\n", cal_voltage, duty * 100.0f);
    cdc_printf("Pole Pairs     : %lu\r\n", motor_config.pole_pairs);
    cdc_printf("Current Offset : %.2f deg\r\n\r\n", motor_config.hall_offset_deg);
    cdc_printf("WARNING: Motor will rotate slowly in both directions.\r\n");
    cdc_printf("Ensure wheel is completely free to spin without load!\r\n\r\n");

    // Pre-check Hall sensor pins
    uint8_t init_hall = Get_Hall_State();
    if (init_hall == 0 || init_hall == 7) {
        cdc_printf("[CAL] ERROR: Invalid Hall State (%d)!\r\n", init_hall);
        cdc_printf("[CAL] Check 5V power, GND, and pull-up resistors on PA0, PA1, PA2.\r\n");
        cdc_printf("==================================================\r\n");
        motor_config.verbose_output = old_verbose;
        return;
    }

    // 2. Pre-charge IR2110 bootstrap capacitors (LIN=1, HIN=0)
    htim1.Instance->CCR1 = 0;
    htim1.Instance->CCR2 = 0;
    htim1.Instance->CCR3 = 0;
    ENABLE_PHASE_U();
    ENABLE_PHASE_V();
    ENABLE_PHASE_W();
    HAL_Delay(10);

    uint32_t arr = htim1.Instance->ARR;

    // 3. Step 1: Lock rotor to Electrical 0 deg
    cdc_printf("[1/3] Locking rotor to Electrical 0 deg...\r\n");
    for (int step = 0; step <= 50; step++) {
        float ramp = duty * ((float)step / 50.0f);
        float ta, tb, tc;
        SVPWM(ramp, 0.0f, &ta, &tb, &tc);
        htim1.Instance->CCR1 = (uint32_t)(ta * arr);
        htim1.Instance->CCR2 = (uint32_t)(tb * arr);
        htim1.Instance->CCR3 = (uint32_t)(tc * arr);
        HAL_Delay(10);
    }
    HAL_Delay(1000); // 1 sec steady hold

    cdc_printf("      Rotor locked. Phase Currents: U=%.2fA, V=%.2fA, W=%.2fA\r\n",
               Get_Current_U(), Get_Current_V(), Get_Current_W());

    // 4. Step 2: Forward Electrical Sweep (0 -> 720 deg, 2 electrical cycles)
    cdc_printf("[2/3] Sweeping Electrical Vector FORWARD (0 -> 720 deg)...\r\n");
    float fwd_edge[8] = {0};
    uint8_t fwd_seen[8] = {0};
    uint8_t fwd_seq[16] = {0};
    uint8_t fwd_seq_count = 0;

    uint8_t prev_hall = Get_Hall_State();
    const int total_steps = 2000; // 2000 * 2ms = 4000ms (4 seconds)
    float delta_theta = (4.0f * M_PI) / (float)total_steps;
    float theta = 0.0f;

    for (int i = 0; i <= total_steps; i++) {
        theta = (float)i * delta_theta;
        float v_alpha = duty * cosf(theta);
        float v_beta  = duty * sinf(theta);
        float ta, tb, tc;
        SVPWM(v_alpha, v_beta, &ta, &tb, &tc);
        htim1.Instance->CCR1 = (uint32_t)(ta * arr);
        htim1.Instance->CCR2 = (uint32_t)(tb * arr);
        htim1.Instance->CCR3 = (uint32_t)(tc * arr);
        HAL_Delay(2);

        // Read Hall state
        uint8_t h_curr = Get_Hall_State();
        if (h_curr != prev_hall && h_curr >= 1 && h_curr <= 6) {
            // In second electrical cycle (theta >= 2*PI), record entry angle
            if (theta >= (2.0f * M_PI)) {
                float theta_deg = fmodf(theta * 180.0f / M_PI, 360.0f);
                if (!fwd_seen[h_curr]) {
                    fwd_edge[h_curr] = theta_deg;
                    fwd_seen[h_curr] = 1;
                    if (fwd_seq_count < 15) {
                        fwd_seq[fwd_seq_count++] = h_curr;
                    }
                }
            }
            prev_hall = h_curr;
        }
    }
    HAL_Delay(300);

    // 5. Step 3: Reverse Electrical Sweep (720 -> 0 deg)
    cdc_printf("[3/3] Sweeping Electrical Vector REVERSE (720 -> 0 deg)...\r\n");
    float rev_edge[8] = {0};
    uint8_t rev_seen[8] = {0};
    prev_hall = Get_Hall_State();

    for (int i = total_steps; i >= 0; i--) {
        theta = (float)i * delta_theta;
        float v_alpha = duty * cosf(theta);
        float v_beta  = duty * sinf(theta);
        float ta, tb, tc;
        SVPWM(v_alpha, v_beta, &ta, &tb, &tc);
        htim1.Instance->CCR1 = (uint32_t)(ta * arr);
        htim1.Instance->CCR2 = (uint32_t)(tb * arr);
        htim1.Instance->CCR3 = (uint32_t)(tc * arr);
        HAL_Delay(2);

        uint8_t h_curr = Get_Hall_State();
        if (h_curr != prev_hall && h_curr >= 1 && h_curr <= 6) {
            // When exiting prev_hall in reverse, that marks the same boundary as entering prev_hall in forward!
            if (theta <= (2.0f * M_PI)) {
                float theta_deg = fmodf(theta * 180.0f / M_PI, 360.0f);
                if (prev_hall >= 1 && prev_hall <= 6 && !rev_seen[prev_hall]) {
                    rev_edge[prev_hall] = theta_deg;
                    rev_seen[prev_hall] = 1;
                }
            }
            prev_hall = h_curr;
        }
    }

    // Ramp down and float phases safely
    for (int step = 30; step >= 0; step--) {
        float ramp = duty * ((float)step / 30.0f);
        float ta, tb, tc;
        SVPWM(ramp, 0.0f, &ta, &tb, &tc);
        htim1.Instance->CCR1 = (uint32_t)(ta * arr);
        htim1.Instance->CCR2 = (uint32_t)(tb * arr);
        htim1.Instance->CCR3 = (uint32_t)(tc * arr);
        HAL_Delay(5);
    }
    SixStep_Stop();

    // 6. Data Validation & Offset Calculation
    int valid_fwd_count = 0;
    int valid_rev_count = 0;
    const uint8_t valid_states[6] = {5, 1, 3, 2, 6, 4};

    for (int idx = 0; idx < 6; idx++) {
        uint8_t s = valid_states[idx];
        if (fwd_seen[s]) valid_fwd_count++;
        if (rev_seen[s]) valid_rev_count++;
    }

    if (valid_fwd_count < 6 || valid_rev_count < 6) {
        cdc_printf("\r\n--------------------------------------------------\r\n");
        cdc_printf("[CAL] ERROR: Incomplete Hall pattern detected!\r\n");
        cdc_printf("Forward states detected: %d/6, Reverse states: %d/6\r\n", valid_fwd_count, valid_rev_count);
        cdc_printf("Possible causes:\r\n");
        cdc_printf(" 1. Calibration voltage too low for hub motor cogging torque.\r\n");
        cdc_printf("    -> Try running with higher voltage: '$cal=3.5' or '$cal=4.0'\r\n");
        cdc_printf(" 2. Broken Hall sensor line or loose connector.\r\n");
        cdc_printf(" 3. Wheel was held or jammed during calibration.\r\n");
        cdc_printf("==================================================\r\n");
        motor_config.verbose_output = old_verbose;
        return;
    }

    // Direction detection
    // Expected normal: 5 -> 1 -> 3 -> 2 -> 6 -> 4 -> ...
    // Expected inverted: 5 -> 4 -> 6 -> 2 -> 3 -> 1 -> ...
    uint8_t detected_inverted = 0;
    if (fwd_seq_count >= 2) {
        if (fwd_seq[0] == 5 && fwd_seq[1] == 4) detected_inverted = 1;
        else if (fwd_seq[0] == 1 && fwd_seq[1] == 5) detected_inverted = 1;
        else if (fwd_seq[0] == 3 && fwd_seq[1] == 1) detected_inverted = 1;
        else if (fwd_seq[0] == 2 && fwd_seq[1] == 3) detected_inverted = 1;
        else if (fwd_seq[0] == 6 && fwd_seq[1] == 2) detected_inverted = 1;
        else if (fwd_seq[0] == 4 && fwd_seq[1] == 6) detected_inverted = 1;
    }

    cdc_printf("\r\n--------------------------------------------------\r\n");
    cdc_printf("                 CALIBRATION RESULT               \r\n");
    cdc_printf("--------------------------------------------------\r\n");
    cdc_printf("Hall Sequence: ");
    for (int i = 0; i < fwd_seq_count; i++) {
        cdc_printf("%d%s", fwd_seq[i], (i < fwd_seq_count - 1) ? " -> " : "");
    }
    cdc_printf("\r\n");
    cdc_printf("Direction    : %s (Setting $14=%d)\r\n\r\n",
               detected_inverted ? "INVERTED" : "NORMAL", detected_inverted);

    cdc_printf("Sector Transitions (Electrical Degrees):\r\n");
    float sum_sin = 0.0f;
    float sum_cos = 0.0f;

    for (int idx = 0; idx < 6; idx++) {
        uint8_t s = valid_states[idx];
        float f_deg = fwd_edge[s];
        float r_deg = rev_edge[s];

        // Circular midpoint
        float f_rad = f_deg * M_PI / 180.0f;
        float r_rad = r_deg * M_PI / 180.0f;
        float mid_rad = atan2f(sinf(f_rad) + sinf(r_rad), cosf(f_rad) + cosf(r_rad));
        float mid_deg = mid_rad * 180.0f / M_PI;
        while (mid_deg < 0.0f) mid_deg += 360.0f;
        while (mid_deg >= 360.0f) mid_deg -= 360.0f;

        float ref_deg = hall_angles[s] * 180.0f / M_PI;

        // Offset = (mid_deg + 90.0) - ref_deg
        float off = mid_deg + 90.0f - ref_deg;
        while (off < 0.0f) off += 360.0f;
        while (off >= 360.0f) off -= 360.0f;

        float off_rad = off * M_PI / 180.0f;
        sum_sin += sinf(off_rad);
        sum_cos += cosf(off_rad);

        float diff = mid_deg - ref_deg;
        if (diff > 180.0f) diff -= 360.0f;
        if (diff < -180.0f) diff += 360.0f;

        cdc_printf("  State %d: Fwd=%5.1f, Rev=%5.1f -> Mid=%5.1f deg (Ref=%5.1f, Diff=%+5.1f)\r\n",
                   s, f_deg, r_deg, mid_deg, ref_deg, diff);
    }

    // Circular vector average across all 6 sectors
    float final_offset_rad = atan2f(sum_sin, sum_cos);
    float final_offset_deg = final_offset_rad * 180.0f / M_PI;
    while (final_offset_deg < 0.0f) final_offset_deg += 360.0f;
    while (final_offset_deg >= 360.0f) final_offset_deg -= 360.0f;

    cdc_printf("\r\nCalculated Hall Offset : %.2f deg\r\n", final_offset_deg);
    cdc_printf("Old Config Offset      : %.2f deg\r\n\r\n", motor_config.hall_offset_deg);

    // Automatically update config in RAM
    motor_config.hall_offset_deg = final_offset_deg;
    motor_config.invert_direction = detected_inverted;

    cdc_printf("SUCCESS: motor_config.hall_offset_deg updated to %.2f deg!\r\n", final_offset_deg);
    cdc_printf("Type '$save' to permanently save this configuration to Flash.\r\n");
    cdc_printf("==================================================\r\n");
    cdc_printf("ok\r\n");

    motor_config.verbose_output = old_verbose;
}
