#include "config.h"
#include <string.h>
#include <stdio.h>

// For STM32F446RE, Sector 7 is 128KB, starting at 0x08060000
#define FLASH_USER_START_ADDR   0x08060000
#define FLASH_USER_SECTOR       FLASH_SECTOR_7

ConfigData current_config;

static void Config_SetDefaults(void) {
    current_config.magic = CONFIG_MAGIC;
    current_config.wheel_diameter_mm = 500.0f; // Default 50cm
    current_config.pulses_per_rev = 1.0f;      // Default 1 pulse per rev
    current_config.coast_speed_min = 15.0f;    // 15 km/h
    current_config.burn_speed_max = 35.0f;     // 35 km/h
}

void Config_Init(void) {
    // Read from flash
    ConfigData *flash_config = (ConfigData*)FLASH_USER_START_ADDR;
    
    if (flash_config->magic == CONFIG_MAGIC) {
        // Valid config found, copy to RAM
        memcpy(&current_config, flash_config, sizeof(ConfigData));
    } else {
        // No valid config, set defaults
        Config_SetDefaults();
    }
}

void Config_Save(void) {
    HAL_FLASH_Unlock();
    
    // Clear pending flags (if any)
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    // Erase Sector
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = FLASH_USER_SECTOR;
    EraseInitStruct.NbSectors = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        HAL_FLASH_Lock();
        return; // Erase failed
    }

    // Write structure word by word (32 bits)
    uint32_t *data_ptr = (uint32_t *)&current_config;
    uint32_t addr = FLASH_USER_START_ADDR;
    uint32_t num_words = sizeof(ConfigData) / 4;
    
    // Pad to word boundary if necessary
    if (sizeof(ConfigData) % 4 != 0) {
        num_words++;
    }

    for (uint32_t i = 0; i < num_words; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data_ptr[i]) == HAL_OK) {
            addr += 4;
        } else {
            // Write failed
            break;
        }
    }

    HAL_FLASH_Lock();
}
