#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define CONFIG_MAGIC_WORD 0xA5A55A5C // Changed slightly from reference to avoid clash if ever ported

// Flash Page 63 (Last page of 128KB flash on STM32G431)
#define CONFIG_FLASH_ADDRESS ((uint32_t)0x0801F800) 

typedef struct {
    uint32_t magic_word;
    
    // Motor Parameters
    uint32_t pwm_freq;           // $0: PWM Frequency in Hz (Default 20000)
    uint32_t pole_pairs;         // $1: Number of pole pairs
    
    // Electrical Parameters
    float dc_bus_voltage;        // $2: DC Bus Voltage
    float max_output_voltage;    // $3: Max Output Voltage
    
    // Debug & Open Loop
    uint32_t verbose_output;     // $4: 0=Off, 1=On
    uint32_t verbose_period;     // $5: Period in ms
    float open_loop_voltage;     // $6: Voltage for Open Loop Mode
    
    float accel_limit;           // $7: Acceleration Limit (rad/s^2), 0 = no limit
    float switchover_rpm;        // $8: Switchover RPM from 6-step to SVPWM
    float switchover_delay;      // $9: Switchover Delay in seconds
    float hall_offset_deg;       // $10: Hall Offset in Degrees
    float vel_kp;                // $11: Velocity Proportional Gain
    float vel_ki;                // $12: Velocity Integral Gain
    float accel_rpm_s;           // $13: Acceleration Limit (RPM/s)
    
    // Future expansion space
    uint32_t reserved[8];
} motor_config_t;

extern motor_config_t motor_config;

// Initialize config, load from flash or set default
void Config_Init(void);

// Save current config to flash
void Config_Save(void);

// Parse incoming GRBL-style command string
void Config_ParseCommand(char* cmd_line);

// Helper for USB CDC printf
void cdc_printf(const char *format, ...);

// High-speed non-blocking binary telemetry for GUI
void Telemetry_SendBinary(float pos, float vel, float vq, float target, float ia, float ib, float ic, uint8_t mode);

// Force PWM and sine table update based on config.pwm_freq
void Config_ApplyPWMFrequency(void);

#endif // CONFIG_H
