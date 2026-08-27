#include "pid.h"

void PID_Init(pid_controller_t *pid, float kp, float ki, float kd, float limit) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->output_limit = limit;
    PID_Reset(pid);
}

void PID_Reset(pid_controller_t *pid) {
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_measurement = 0.0f;
}

float PID_Update(pid_controller_t *pid, float setpoint, float measurement, float dt) {
    float error = setpoint - measurement;
    
    // Proportional term
    float p_out = pid->kp * error;
    
    // Integral term with anti-windup clamping
    pid->integral += error * dt;
    float i_out = pid->ki * pid->integral;
    
    // Clamp integral
    if (i_out > pid->output_limit) {
        i_out = pid->output_limit;
        pid->integral = i_out / pid->ki;
    } else if (i_out < -pid->output_limit) {
        i_out = -pid->output_limit;
        pid->integral = i_out / pid->ki;
    }
    
    // Derivative term (on measurement to avoid derivative kick)
    float d_out = -pid->kd * (measurement - pid->prev_measurement) / dt;
    
    // Compute total output
    float output = p_out + i_out + d_out;
    
    // Clamp output
    if (output > pid->output_limit) output = pid->output_limit;
    else if (output < -pid->output_limit) output = -pid->output_limit;
    
    // Store variables for next iteration
    pid->prev_error = error;
    pid->prev_measurement = measurement;
    
    return output;
}
