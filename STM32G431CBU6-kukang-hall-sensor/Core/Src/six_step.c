#include "six_step.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;

// Motor state variables
static float current_duty = 0.0f; // 0.0 to 100.0
static uint8_t motor_running = 0;
static uint8_t last_hall_state = 0;

void SixStep_Init(void) {
    current_duty = 0.0f;
    motor_running = 0;
    last_hall_state = 0;
    SixStep_Stop();
}

void SixStep_SetSpeed(float target_duty) {
    if (target_duty < 0.0f) target_duty = 0.0f;
    if (target_duty > 100.0f) target_duty = 100.0f;
    
    current_duty = target_duty;
    
    if (current_duty > 0.1f) {
        // Only start if not already running to force a state re-evaluation
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
    
    // Disable all PWMs
    htim1.Instance->CCR1 = 0;
    htim1.Instance->CCR2 = 0;
    htim1.Instance->CCR3 = 0;
    
    // Disable all EN pins (floating)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14 | GPIO_PIN_15, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
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
    
    // Only commutate if state changed to avoid writing registers unnecessarily
    if (hall_state == last_hall_state) {
        // If duty cycle changed, we still need to update CCR without changing the active phase.
        // For simplicity, we just update CCR1/2/3 globally based on current_duty.
        // The switch block handles this perfectly if we just remove the early return,
        // but performance is better if we only update when needed.
        // Since SetSpeed handles dynamic speed changes slowly, we can just let it update on the next hall step.
        // However, if standing still, we need to apply the new duty cycle.
        // We will just do a fast return if neither hall state nor duty changed.
    }
    
    // Update CCR based on duty cycle
    uint32_t arr = htim1.Instance->ARR;
    uint32_t ccr_val = (uint32_t)((current_duty / 100.0f) * arr);

    last_hall_state = hall_state;

    // Commutation Table (Standard 120-degree ebike motor)
    // 1 = High (PWM), 0 = Low (GND, CCR=0), X = Float (EN=0)
    // Phase U = CH1, EN_U = PB14
    // Phase V = CH2, EN_V = PB15
    // Phase W = CH3, EN_W = PC6
    
    switch (hall_state) {
        case 5:
            // U=High, V=Low, W=Float
            htim1.Instance->CCR1 = ccr_val;
            htim1.Instance->CCR2 = 0;
            htim1.Instance->CCR3 = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);   // EN_U = 1
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);   // EN_V = 1
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);  // EN_W = 0
            break;
        case 1:
            // U=High, V=Float, W=Low
            htim1.Instance->CCR1 = ccr_val;
            htim1.Instance->CCR2 = 0;
            htim1.Instance->CCR3 = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);   // EN_U = 1
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET); // EN_V = 0
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);    // EN_W = 1
            break;
        case 3:
            // U=Float, V=High, W=Low
            htim1.Instance->CCR1 = 0;
            htim1.Instance->CCR2 = ccr_val;
            htim1.Instance->CCR3 = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET); // EN_U = 0
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);   // EN_V = 1
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);    // EN_W = 1
            break;
        case 2:
            // U=Low, V=High, W=Float
            htim1.Instance->CCR1 = 0;
            htim1.Instance->CCR2 = ccr_val;
            htim1.Instance->CCR3 = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);   // EN_U = 1
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);   // EN_V = 1
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);  // EN_W = 0
            break;
        case 6:
            // U=Low, V=Float, W=High
            htim1.Instance->CCR1 = 0;
            htim1.Instance->CCR2 = 0;
            htim1.Instance->CCR3 = ccr_val;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);   // EN_U = 1
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET); // EN_V = 0
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);    // EN_W = 1
            break;
        case 4:
            // U=Float, V=Low, W=High
            htim1.Instance->CCR1 = 0;
            htim1.Instance->CCR2 = 0;
            htim1.Instance->CCR3 = ccr_val;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET); // EN_U = 0
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);   // EN_V = 1
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);    // EN_W = 1
            break;
        default:
            // Invalid state (0 or 7), float everything to prevent shorts
            htim1.Instance->CCR1 = 0;
            htim1.Instance->CCR2 = 0;
            htim1.Instance->CCR3 = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14 | GPIO_PIN_15, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
            break;
    }
}

// For debugging via CDC
extern void cdc_printf(const char *format, ...);
void SixStep_PrintVerbose(void) {
    cdc_printf("Status: %s, Duty: %.1f%%, Hall: %d\r\n", 
               motor_running ? "RUN" : "STOP", current_duty, last_hall_state);
}
