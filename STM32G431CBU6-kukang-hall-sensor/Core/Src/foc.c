#include "foc.h"
#include "config.h"
#include "main.h"
#include "hub_motor_ebike.h"
#include "pid.h"
#include <math.h>

foc_state_t foc;
pid_controller_t pid_vel;
pid_controller_t pid_pos;
pid_controller_t pid_id;
pid_controller_t pid_iq;

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern uint32_t SystemCoreClock;

#define PI 3.14159265359f
#define SQRT3_2 0.86602540378f

// Internal variables for velocity calculation
static float vel_lpf = 0.0f;       // Low-pass filtered velocity
static uint32_t align_counter = 0; // Countdown for alignment
static int32_t total_count = 0;    // Continuous 32-bit count
#if (ENCODER_USE != ENCODER_TYPE_NONE)
static uint16_t last_cnt = 0;      // Last 16-bit CNT
#endif

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

void FOC_Init(void) {
  // Configure CORDIC for Fast Sine/Cosine
  // FUNC = 0 (Cosine/Sine), PRECISION = 5 (24-bit, 6 cycles)
  // SCALE = 0, IEN = 0, NRES = 1 (2 results), NARGS = 0 (1 arg)
  CORDIC->CSR = (0x00 << CORDIC_CSR_FUNC_Pos) |
                (5 << CORDIC_CSR_PRECISION_Pos) |
                (1 << CORDIC_CSR_NRES_Pos) | 
                (0 << CORDIC_CSR_NARGS_Pos);


  foc.electrical_angle = 0.0f;
  foc.mechanical_angle = 0.0f;
  foc.shaft_velocity = 0.0f;
  foc.v_d = 0.0f;
  foc.v_q = 0.0f;
  foc.target_voltage = 0.0f;
  foc.target_velocity = 0.0f;
  foc.target_position = 0.0f;
  foc.ramped_target_velocity = 0.0f;
  foc.open_loop_angle = 0.0f;
  foc.enabled = 0;

  // Initialize PIDs with config parameters
  PID_Init(&pid_vel, motor_config.vel_kp, motor_config.vel_ki,
           motor_config.vel_kd, motor_config.vel_limit);
  PID_Init(&pid_pos, motor_config.pos_kp, motor_config.pos_ki,
           motor_config.pos_kd, motor_config.pos_limit);
  PID_Init(&pid_id, motor_config.current_kp, motor_config.current_ki, 0.0f,
           motor_config.current_limit);
  PID_Init(&pid_iq, motor_config.current_kp, motor_config.current_ki, 0.0f,
           motor_config.current_limit);

#if (ENCODER_USE == ENCODER_TYPE_INCREMENTAL)
  // Reset and start encoder
  htim3.Instance->CNT = 0;
  total_count = 0;
  last_cnt = 0;
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
#elif (ENCODER_USE == ENCODER_TYPE_AS5600)
  total_count = 0;
  last_cnt = as5600_raw_angle;
#endif

  vel_lpf = 0.0f;
  align_counter = 0;
}

void FOC_SetTarget(float target) {
  if (motor_config.control_mode == 0) {
    foc.target_current_q =
        target; // target_voltage parameter is now used as target current (Amps)
    foc.target_voltage = target;
  } else if (motor_config.control_mode == 1) {
    foc.target_velocity = target;

  } else if (motor_config.control_mode == 2) {
    foc.target_position = target;
  } else if (motor_config.control_mode == 3) {
    foc.target_velocity =
        target; // use target_velocity to drive the open loop angle
  }
}

void FOC_Align(void) {
  if (motor_config.pwm_freq > 0) {
    // Align for 2 seconds
    align_counter = 2 * motor_config.pwm_freq;

    extern void enable_mosfet(void);
    enable_mosfet();
  }
}

// Calculate modulo for float to stay within [0, 2PI]
	static float _wrap_2pi(float angle) {
  while (angle < 0.0f)
    angle += 2.0f * PI;
  while (angle > 2.0f * PI)
    angle -= 2.0f * PI;
  return angle;
}

void FOC_UpdateEncoder(float dt) {
#if (ENCODER_USE == ENCODER_TYPE_INCREMENTAL)
  // Encoder mode T1 and T2 means X4 resolution (4 counts per pulse)
  // CPR = PPR * 4
  int32_t cpr = (int32_t)(motor_config.encoder_ppr * 4);
  if (cpr == 0)
    return;

  uint16_t current_cnt = (uint16_t)htim3.Instance->CNT;
  int16_t delta = (int16_t)(current_cnt - last_cnt);

#elif (ENCODER_USE == ENCODER_TYPE_AS5600)
  int32_t cpr = AS5600_CPR;                // 4096
  uint16_t current_cnt = as5600_raw_angle; // 0..4095

  // Calculate shortest distance delta considering wrap around at 4096
  int16_t delta = (int16_t)current_cnt - (int16_t)last_cnt;
  if (delta > 2048)
    delta -= 4096;
  else if (delta < -2048)
    delta += 4096;
#endif

#if (ENCODER_USE != ENCODER_TYPE_NONE)
  last_cnt = current_cnt;
  total_count += delta;

  // Apply encoder direction
  float continuous_count = (float)total_count;
  int16_t dir_delta = delta;
  if (motor_config.encoder_dir == 1) {
    continuous_count = -continuous_count;
    dir_delta = -dir_delta;
  }

  // Mechanical angle (continuous multi-turn)
  foc.mechanical_angle = (continuous_count / (float)cpr) * 2.0f * PI;

  // Electrical angle (wrapped to 0-2PI)
  float elec_angle = foc.mechanical_angle * motor_config.pole_pairs;
  elec_angle = fmodf(elec_angle, 2.0f * PI);
  if (elec_angle < 0.0f) {
    elec_angle += 2.0f * PI;
  }
  foc.electrical_angle = elec_angle;

  // Shaft velocity (rad/s)
  if (dt > 0.0f) {
    float raw_vel = ((float)dir_delta / (float)cpr) * 2.0f * PI / dt;

    // Simple low-pass filter to smooth velocity
    // Alpha = dt / (tau + dt) -> for cutoff ~8Hz, tau = 0.02
    float alpha = dt / (0.02f + dt);
    vel_lpf = (1.0f - alpha) * vel_lpf + alpha * raw_vel;
    foc.shaft_velocity = vel_lpf;
  }
#endif
}

void FOC_Update(void) {
  static uint32_t last_cycle_count = 0;
  uint32_t current_time = DWT->CYCCNT;
  uint32_t cycle_diff = current_time - last_cycle_count;
  last_cycle_count = current_time;

  float dt = (float)cycle_diff / (float)SystemCoreClock;
  if (dt <= 0.000001f) {
    dt = 0.00005f; // Fallback to 20kHz nominal dt to prevent NaN
  }

  if (dt <= 0.0f)
    return;

  FOC_UpdateEncoder(dt);

  // Phase Current Processing
  // Filter ADC offset continuously when motor is disabled
  // (Disabled so user can manually set offsets via $24, $25, $26)
  /*
  if (!foc.enabled) {
    float alpha = 0.05f;
    motor_config.current_sense_offset_a =
        (1.0f - alpha) * motor_config.current_sense_offset_a +
        alpha * (float)foc.raw_i_a;
    motor_config.current_sense_offset_b =
        (1.0f - alpha) * motor_config.current_sense_offset_b +
        alpha * (float)foc.raw_i_b;
    motor_config.current_sense_offset_c =
        (1.0f - alpha) * motor_config.current_sense_offset_c +
        alpha * (float)foc.raw_i_c;
  }
  */

  // ==============================================================================
  // VOLTAGE-GUIDED CURRENT OBSERVER (State Estimator for BTS7960 Half-Wave Sensors)
  // ==============================================================================
  
  // 1. Read raw sensors and eliminate negative noise
  float ib_raw = ((float)foc.raw_i_b - motor_config.current_sense_offset_b) * motor_config.current_sense_gain;
  float ic_raw = ((float)foc.raw_i_c - motor_config.current_sense_offset_c) * motor_config.current_sense_gain;

  float ib_sens = (ib_raw > 0.0f) ? ib_raw : 0.0f;
  float ic_sens = (ic_raw > 0.0f) ? ic_raw : 0.0f;

  // 2. Feedforward Current Angle (Aligned to Q-Axis for Torque)
  // We use CORDIC to calculate sin/cos of electrical_angle just ONCE!
  float s, c;
  CORDIC_Fast_SinCos(foc.electrical_angle, &s, &c);
  
  // Q-axis is at +90 degrees from D-axis (electrical angle)
  // cos(elec + 90) = -sin(elec), sin(elec + 90) = cos(elec)
  float q_cos = -s;
  float q_sin = c;

  // 3. Feedback Magnitude Observer using Gradient Descent (PLL)
  static float obs_i_mag = 0.0f;
  
  // shape_b = cos(q_angle - 120) = q_cos*cos(-120) - q_sin*sin(-120)
  float shape_b = -0.5f * q_cos + SQRT3_2 * q_sin; 
  // shape_c = cos(q_angle + 120) = q_cos*cos(120) - q_sin*sin(120)
  float shape_c = -0.5f * q_cos - SQRT3_2 * q_sin; 
  
  // Only trust sensors when they should be strongly positive
  // Gradient descent automatically handles both positive and negative Iq!
  if (ib_sens > 0.1f && fabsf(shape_b) > 0.2f) {
      float error = ib_sens - (obs_i_mag * shape_b);
      obs_i_mag += 0.02f * error * shape_b; // 0.02f = learning rate
  }
  if (ic_sens > 0.1f && fabsf(shape_c) > 0.2f) {
      float error = ic_sens - (obs_i_mag * shape_c);
      obs_i_mag += 0.02f * error * shape_c;
  }
  
  // Decay magnitude to 0 if no voltage is applied
  if (fabsf(foc.v_q) < 0.1f && fabsf(foc.v_d) < 0.1f) {
      obs_i_mag *= 0.95f; 
  }

  // 4. Synthesize full continuous sine waves for ALL phases using Observer State!
  foc.i_a = obs_i_mag * q_cos;
  foc.i_b = obs_i_mag * shape_b;
  foc.i_c = obs_i_mag * shape_c;

  // 5. Clarke Transform (using the perfectly reconstructed waves)
  foc.i_alpha = foc.i_a;
  foc.i_beta = (1.0f / SQRT3_2) * 0.5f * (foc.i_b - foc.i_c);

  // Park Transform (Reuses CORDIC results s and c)
  float raw_id = foc.i_alpha * c + foc.i_beta * s;
  float raw_iq = -foc.i_alpha * s + foc.i_beta * c;
  
  // Low-pass filter for Current Feedback (Id, Iq)
  // Alpha = dt / (tau + dt) -> for cutoff ~16Hz, tau = 0.01
  float i_alpha = dt / (0.01f + dt);
  foc.i_d = (1.0f - i_alpha) * foc.i_d + i_alpha * raw_id;
  foc.i_q = (1.0f - i_alpha) * foc.i_q + i_alpha * raw_iq;

  if (align_counter > 0) {

    // Alignment Mode: Force D-axis field and 0 angle
    foc.v_d = motor_config.open_loop_voltage;
    foc.v_q = 0.0f;
    foc.electrical_angle = 0.0f;

    align_counter--;
    if (align_counter == 0) {
      // Alignment complete, reset encoder
#if (ENCODER_USE == ENCODER_TYPE_INCREMENTAL)
      htim3.Instance->CNT = 0;
      last_cnt = 0;
#elif (ENCODER_USE == ENCODER_TYPE_AS5600)
      last_cnt = as5600_raw_angle;
#endif
      total_count = 0;

      foc.target_voltage = 0.0f;
      foc.target_velocity = 0.0f;
      foc.target_position = 0.0f;
      foc.ramped_target_velocity = 0.0f;
      foc.open_loop_angle = 0.0f;

      // Turn off MOSFET after alignment to be safe
      extern void disable_mosfet(void);
      disable_mosfet();
    }
  } else {
    if (foc.enabled) {
      // Cascading PID logic
      if (motor_config.control_mode == 2) {
        // Position Mode -> calculates target velocity
        foc.target_velocity =
            PID_Update(&pid_pos, foc.target_position, foc.mechanical_angle, dt);
      }

      if (motor_config.control_mode == 1 || motor_config.control_mode == 2) {
        if (motor_config.accel_limit > 0.0f) {
          float max_dv = motor_config.accel_limit * dt;
          if (foc.target_velocity > foc.ramped_target_velocity + max_dv) {
            foc.ramped_target_velocity += max_dv;
          } else if (foc.target_velocity <
                     foc.ramped_target_velocity - max_dv) {
            foc.ramped_target_velocity -= max_dv;
          } else {
            foc.ramped_target_velocity = foc.target_velocity;
          }
        } else {
          foc.ramped_target_velocity = foc.target_velocity;
        }

        // Velocity Mode -> calculates target current Iq
        foc.target_current_q = PID_Update(&pid_vel, foc.ramped_target_velocity,
                                          foc.shaft_velocity, dt);
      }

      if (motor_config.control_mode == 3) {
        // Open Loop Mode
        foc.open_loop_angle += foc.target_velocity * dt;
        foc.open_loop_angle = _wrap_2pi(foc.open_loop_angle);

        foc.electrical_angle =
            _wrap_2pi(foc.open_loop_angle * motor_config.pole_pairs);
        foc.v_q = motor_config.open_loop_voltage;
        foc.v_d = 0.0f;
      } else {
        // Closed Loop Current Control (Torque Mode)
        // Iq target is set by Velocity PI or directly in Torque mode
        if (motor_config.current_kp == 0.0f) {
          // Voltage Torque Mode fallback for unsupported current sensors (e.g.
          // BTS7960)
          foc.v_q =
              foc.target_current_q; // target_current_q acts as target voltage
          foc.v_d = 0.0f;
        } else {
          foc.v_q = PID_Update(&pid_iq, foc.target_current_q, foc.i_q, dt);
          foc.v_d = PID_Update(&pid_id, 0.0f, foc.i_d, dt);
        }
      }
    } else {
      // Motor is disabled, reset PIDs to prevent integral windup
      PID_Reset(&pid_pos);
      PID_Reset(&pid_vel);
      PID_Reset(&pid_id);
      PID_Reset(&pid_iq);
      foc.v_q = 0.0f;
      foc.v_d = 0.0f;
      foc.ramped_target_velocity =
          foc.shaft_velocity; // Match actual speed so there is no jerk upon
                              // enable
    }
  }

  // --- Inverse Park Transform ---
  // Note: We already calculated sine and cosine in the Park transform above,
  // but in Open Loop mode electrical_angle is updated *after* Park transform.
  // So we recalculate it using CORDIC to be safe.
  CORDIC_Fast_SinCos(foc.electrical_angle, &s, &c);

  float v_alpha = foc.v_d * c - foc.v_q * s;
  float v_beta = foc.v_d * s + foc.v_q * c;

  // --- Inverse Clarke Transform ---
  foc.u_a = v_alpha;
  foc.u_b = -0.5f * v_alpha + SQRT3_2 * v_beta;
  foc.u_c = -0.5f * v_alpha - SQRT3_2 * v_beta;

  // --- Space Vector PWM (SVPWM) Midpoint Clamping ---
  // Find min and max of phase voltages
  float v_min = foc.u_a;
  if (foc.u_b < v_min)
    v_min = foc.u_b;
  if (foc.u_c < v_min)
    v_min = foc.u_c;

  float v_max = foc.u_a;
  if (foc.u_b > v_max)
    v_max = foc.u_b;
  if (foc.u_c > v_max)
    v_max = foc.u_c;

  // Center the modulation wave
  float v_neutral = 0.5f * (v_min + v_max);

  foc.u_a -= v_neutral;
  foc.u_b -= v_neutral;
  foc.u_c -= v_neutral;

  // Scale to duty cycle (0.0 to 1.0)
  // Maximum phase voltage is dc_bus_voltage / 2
  float v_bus = motor_config.dc_bus_voltage;
  if (v_bus < 1.0f)
    v_bus = 1.0f; // protect against division by zero

  foc.duty_a = (foc.u_a / v_bus) + 0.5f;
  foc.duty_b = (foc.u_b / v_bus) + 0.5f;
  foc.duty_c = (foc.u_c / v_bus) + 0.5f;

  // Constrain duty cycles
  if (foc.duty_a < 0.0f)
    foc.duty_a = 0.0f;
  if (foc.duty_a > 1.0f)
    foc.duty_a = 1.0f;
  if (foc.duty_b < 0.0f)
    foc.duty_b = 0.0f;
  if (foc.duty_b > 1.0f)
    foc.duty_b = 1.0f;
  if (foc.duty_c < 0.0f)
    foc.duty_c = 0.0f;
  if (foc.duty_c > 1.0f)
    foc.duty_c = 1.0f;

  // Apply to TIM1 CCR
  uint32_t period =
      htim1.Instance->ARR + 1; // Actually, for center aligned, Period is ARR
  htim1.Instance->CCR1 = (uint32_t)(foc.duty_a * period);
  htim1.Instance->CCR2 = (uint32_t)(foc.duty_b * period);
  htim1.Instance->CCR3 = (uint32_t)(foc.duty_c * period);
}

void FOC_PrintVerbose(void) {
  if (motor_config.verbose_output == 0)
    return;

  // Use the new high-speed non-blocking binary telemetry
  extern void Telemetry_SendBinary(float pos, float vel, float vq, float target, float ia, float ib, float ic,
                                   uint8_t mode);

  float target = 0.0f;
  if (motor_config.control_mode == 0)
    target = foc.target_current_q;
  else if (motor_config.control_mode == 1 || motor_config.control_mode == 3)
    target = foc.target_velocity;
  else if (motor_config.control_mode == 2)
    target = foc.target_position;

  // Send actual torque (foc.i_q) instead of v_q
  Telemetry_SendBinary(foc.mechanical_angle, foc.shaft_velocity, foc.i_q,
                       target, foc.i_a, foc.i_b, foc.i_c, motor_config.control_mode);
}
