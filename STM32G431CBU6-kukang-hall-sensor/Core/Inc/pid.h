#ifndef PID_H
#define PID_H

typedef struct {
    float kp;
    float ki;
    float kd;
    float output_limit;
    
    float integral;
    float prev_error;
    float prev_measurement;
} pid_controller_t;

void PID_Init(pid_controller_t *pid, float kp, float ki, float kd, float limit);
float PID_Update(pid_controller_t *pid, float setpoint, float measurement, float dt);
void PID_Reset(pid_controller_t *pid);

#endif // PID_H
