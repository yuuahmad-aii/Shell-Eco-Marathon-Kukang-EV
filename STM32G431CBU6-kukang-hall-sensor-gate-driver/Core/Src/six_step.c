#include "six_step.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;

// Motor state variables
static float current_duty = 0.0f; // 0.0 to 100.0
static uint8_t motor_running = 0;
static uint8_t last_hall_state = 0;

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
    
    // Disable outputs first
    SixStep_Stop();
    
    // Start PWM generators
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
    
    // Ensure Main Output Enable (MOE) is set
    htim1.Instance->BDTR |= TIM_BDTR_MOE;
}

void SixStep_SetSpeed(float target_duty) {
    if (target_duty < 0.0f) target_duty = 0.0f;
    if (target_duty > 100.0f) target_duty = 100.0f;
    
    current_duty = target_duty;
    
    if (current_duty > 0.1f) {
        if (!motor_running) {
            last_hall_state = 0; 
        }
        motor_running = 1;
    } else {
        SixStep_Stop();
    }
}

void SixStep_Stop(void) {
    motor_running = 0;
    
    // Disable all PWM and EN pins to Float the motor
    DISABLE_PHASE_U();
    DISABLE_PHASE_V();
    DISABLE_PHASE_W();
    
    htim1.Instance->CCR1 = 0;
    htim1.Instance->CCR2 = 0;
    htim1.Instance->CCR3 = 0;
}

// Hall sensor reading
static uint8_t Get_Hall_State(void) {
    uint8_t state = 0;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) state |= 1;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) state |= 2;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET) state |= 4;
    return state;
}

void SixStep_Update(void) {
    if (!motor_running) return;

    uint8_t hall_state = Get_Hall_State();
    
    // Only commutate if state changed (or forced on first run)
    if (hall_state == last_hall_state) return;

    // Calculate CCR value based on duty cycle
    uint32_t arr = htim1.Instance->ARR;
    uint32_t ccr_val = (uint32_t)((current_duty / 100.0f) * arr);

    last_hall_state = hall_state;

    // Commutation Table for 6-PWM (Standard 120-degree ebike motor)
    // 1 = High (CCR=ccr_val), 0 = Low (GND, CCR=0), X = Float (CCER disabled)
    
    switch (hall_state) {
        case 5:
            // U=High, V=Low, W=Float
            DISABLE_PHASE_W();
            htim1.Instance->CCR1 = ccr_val;
            htim1.Instance->CCR2 = 0;
            ENABLE_PHASE_U();
            ENABLE_PHASE_V();
            break;
        case 1:
            // U=High, V=Float, W=Low
            DISABLE_PHASE_V();
            htim1.Instance->CCR1 = ccr_val;
            htim1.Instance->CCR3 = 0;
            ENABLE_PHASE_U();
            ENABLE_PHASE_W();
            break;
        case 3:
            // U=Float, V=High, W=Low
            DISABLE_PHASE_U();
            htim1.Instance->CCR2 = ccr_val;
            htim1.Instance->CCR3 = 0;
            ENABLE_PHASE_V();
            ENABLE_PHASE_W();
            break;
        case 2:
            // U=Low, V=High, W=Float
            DISABLE_PHASE_W();
            htim1.Instance->CCR1 = 0;
            htim1.Instance->CCR2 = ccr_val;
            ENABLE_PHASE_U();
            ENABLE_PHASE_V();
            break;
        case 6:
            // U=Low, V=Float, W=High
            DISABLE_PHASE_V();
            htim1.Instance->CCR1 = 0;
            htim1.Instance->CCR3 = ccr_val;
            ENABLE_PHASE_U();
            ENABLE_PHASE_W();
            break;
        case 4:
            // U=Float, V=Low, W=High
            DISABLE_PHASE_U();
            htim1.Instance->CCR2 = 0;
            htim1.Instance->CCR3 = ccr_val;
            ENABLE_PHASE_V();
            ENABLE_PHASE_W();
            break;
        default:
            // Invalid state (0 or 7), coast
            SixStep_Stop();
            motor_running = 1; // Keep running flag so it resumes when valid
            break;
    }
}

// For debugging via CDC
extern void cdc_printf(const char *format, ...);
void SixStep_PrintVerbose(void) {
    cdc_printf("Status: %s, Duty: %.1f%%, Hall: %d\r\n", 
               motor_running ? "RUN" : "STOP", current_duty, last_hall_state);
}
