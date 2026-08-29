#ifndef OPEN_LOOP_H
#define OPEN_LOOP_H

#include <stdint.h>

typedef struct {
    float electrical_angle;
    float mechanical_angle; // virtual mechanical angle based on open loop
    float shaft_velocity;   // actual running velocity
    float target_velocity;  // desired velocity from user
    float ramped_target_velocity; // accel limited velocity
    
    float v_q;
    float v_d;
    
    // PWM duty cycles (0.0 to 1.0)
    float duty_a;
    float duty_b;
    float duty_c;
    
    // Phase voltages
    float u_a;
    float u_b;
    float u_c;
    
    uint8_t enabled;
} open_loop_state_t;

extern open_loop_state_t open_loop;

// Initialize the open loop controller
void OpenLoop_Init(void);

// Set the target velocity in rad/s
void OpenLoop_SetTarget(float velocity_rad_s);

// Update the controller (called periodically from main loop)
// dt: time elapsed since last call in seconds
void OpenLoop_Update(float dt);

// Enable the gate driver outputs
void enable_mosfet(void);

// Disable the gate driver outputs
void disable_mosfet(void);

#endif // OPEN_LOOP_H
