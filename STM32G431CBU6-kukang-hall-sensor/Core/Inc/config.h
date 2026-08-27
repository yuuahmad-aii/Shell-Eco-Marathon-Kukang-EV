#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define CONFIG_MAGIC_WORD 0xA5A55A5B

// Flash Page 63 (Last page of 128KB flash on STM32G431)
#define CONFIG_FLASH_ADDRESS ((uint32_t)0x0801F800) 

typedef struct {
    uint32_t magic_word;
    
    // Motor Parameters
    uint32_t pwm_freq;           // $0: PWM Frequency in Hz (Default 20000)
    uint32_t pole_pairs;         // $1: Number of pole pairs
    uint32_t magnets;            // $2: Number of magnets
    uint32_t encoder_ppr;        // $3: Encoder Resolution (PPR)
    
    // Electrical Parameters
    float dc_bus_voltage;        // $4: DC Bus Voltage
    float max_output_voltage;    // $5: Max Output Voltage
    
    // Control Mode
    uint32_t control_mode;       // $6: 0=Torque(Voltage), 1=Velocity, 2=Position
    
    // Velocity PID
    float vel_kp;                // $7
    float vel_ki;                // $8
    float vel_kd;                // $9
    float vel_limit;             // $10
    
    // Position PID
    float pos_kp;                // $11
    float pos_ki;                // $12
    float pos_kd;                // $13
    float pos_limit;             // $14
    
    // Hardware Setup
    uint32_t encoder_dir;        // $15: 0=Normal, 1=Inverted
    
    // Debug & Open Loop
    uint32_t verbose_output;     // $16: 0=Off, 1=On
    uint32_t verbose_period;     // $17: Period in ms
    float open_loop_voltage;     // $18: Voltage for Open Loop Mode ($6=3)
    
    float accel_limit;           // $19: Acceleration Limit (rad/s^2), 0 = no limit
    
    // Current PID
    float current_kp;            // $20
    float current_ki;            // $21
    float current_limit;         // $22
    
    float current_sense_gain;    // $23: Amps per ADC unit
    float current_sense_offset_a; // $24: ADC Offset Phase A
    float current_sense_offset_b; // $25: ADC Offset Phase B
    float current_sense_offset_c; // $26: ADC Offset Phase C
    
    // Future expansion space (to avoid structure size changing frequently)
    uint32_t reserved[1]; // Adjusted padding
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
