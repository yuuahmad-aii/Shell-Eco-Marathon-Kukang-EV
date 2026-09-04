#ifndef __CONFIG_H
#define __CONFIG_H

#include "main.h"

// Magic word to check if flash has valid config
#define CONFIG_MAGIC 0x12345678

// Storage structure for GRBL parameters
typedef struct {
    uint32_t magic;
    
    // Parameters
    float wheel_diameter_mm;  // $10
    float pulses_per_rev;     // $11
    float coast_speed_min;    // $20
    float burn_speed_max;     // $21
} ConfigData;

extern ConfigData current_config;

void Config_Init(void);
void Config_Save(void);

#endif // __CONFIG_H
