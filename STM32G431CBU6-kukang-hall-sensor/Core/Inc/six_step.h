#ifndef SIX_STEP_H
#define SIX_STEP_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

// Initialize 6-step commutation variables
void SixStep_Init(void);

// Update the commutation based on Hall sensors. Call this in while(1) loop.
void SixStep_Update(void);

// Set speed (0.0 to 100.0)
void SixStep_SetSpeed(float target_duty);

// Stop the motor immediately (coast)
void SixStep_Stop(void);

// Print debug info
void SixStep_PrintVerbose(void);

#endif // SIX_STEP_H
