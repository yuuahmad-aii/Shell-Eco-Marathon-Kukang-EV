#ifndef FOC_H
#define FOC_H

#include <stdint.h>
#include "pid.h"

// FOC state variables
typedef struct {
    // Electrical parameters
    float electrical_angle; // radians
    float mechanical_angle; // radians
    float shaft_velocity;   // rad/s
    
    // Voltages
    float v_d;
    float v_q;
    
    // Phase voltages
    float u_a;
    float u_b;
    float u_c;
    
    // Currents
    uint16_t raw_i_a;
    uint16_t raw_i_b;
    uint16_t raw_i_c;
    
    float i_a;
    float i_b;
    float i_c;
    
    float i_alpha;
    float i_beta;
    
    float i_d;
    float i_q;
    
    // PWM duty cycles (0.0 to 1.0)
    float duty_a;
    float duty_b;
    float duty_c;
    
    // Targets
    float target_voltage;   // Open loop voltage or generic target
    float target_current_q; // Target Iq for torque control
    float target_velocity;
    float target_position;
    
    float ramped_target_velocity; // internal variable for acceleration limit
    
    // Open loop
    float open_loop_angle; // mechanical radians
    
    uint8_t enabled; // Motor enabled flag
    
} foc_state_t;

extern foc_state_t foc;
extern pid_controller_t pid_vel;
extern pid_controller_t pid_pos;
extern pid_controller_t pid_id;
extern pid_controller_t pid_iq;

// Initialize FOC and PID controllers
void FOC_Init(void);

// Calculate electrical angle based on encoder reading
void FOC_UpdateEncoder(float dt);

// Main FOC loop (to be called inside TIM1 interrupt)
void FOC_Update(void);

// Setters for external control
void FOC_SetTarget(float target);

// Motor alignment routine
void FOC_Align(void);

// Verbose output
void FOC_PrintVerbose(void);

#endif // FOC_H
