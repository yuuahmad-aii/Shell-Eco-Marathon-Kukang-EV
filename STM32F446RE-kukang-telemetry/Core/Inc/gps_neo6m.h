#ifndef __GPS_NEO6M_H
#define __GPS_NEO6M_H

#include "main.h"

// Struct to store GPS data
typedef struct {
    float latitude;
    float longitude;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t is_valid;
    uint8_t is_time_valid;
    uint8_t fix_type;
    uint8_t num_satellites;
    float pdop;
    float gps_altitude;
    char sw_version[32];
    char hw_version[12];
} GPS_Data;

// Function Prototypes
void GPS_Init(UART_HandleTypeDef *huart);
void GPS_UART_RxCpltCallback(UART_HandleTypeDef *huart);
uint8_t GPS_GetLatestData(GPS_Data *data);

#endif /* __GPS_NEO6M_H */
