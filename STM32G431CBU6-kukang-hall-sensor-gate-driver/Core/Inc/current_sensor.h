#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include "stm32g4xx_hal.h"

// Define the voltage divider ratio used on the ACS712 5V output to 3.3V ADC.
// Assuming 3.3k resistor is connected to GND and 2.2k is in series: Ratio = 3.3 / (2.2 + 3.3) = 0.6
// Assuming 2.2k resistor is connected to GND and 3.3k is in series: Ratio = 2.2 / (2.2 + 3.3) = 0.4
// Change this macro according to the actual hardware wiring!
#define VOLTAGE_DIVIDER_RATIO 0.4f

// ACS712 30A sensitivity is 66mV per Ampere (0.066 V/A).
#define ACS712_30A_SENSITIVITY 0.066f

// Number of samples to average during startup for zero-current offset calibration
#define CALIBRATION_SAMPLES 2000

// ADC Reference Voltage
#define ADC_VREF 3.3f

// ADC Max Value for 12-bit
#define ADC_MAX_VAL 4095.0f

// DC Bus Voltage divider resistors (PC4 -> ADC2_IN5)
// R1 = 100k Ohm connected to Vbus, R2 = 4.7k Ohm connected to GND
#define VBUS_R1_OHMS 100000.0f
#define VBUS_R2_OHMS 4700.0f
#define VBUS_DIVIDER_RATIO ((VBUS_R1_OHMS + VBUS_R2_OHMS) / VBUS_R2_OHMS) // 104.7 / 4.7 = 22.2766f

void CurrentSensor_Init(void);

// Get currents in Amperes
float Get_Current_U(void);
float Get_Current_V(void);
float Get_Current_W(void);

// Get FOC Currents (Amperes)
float Get_Current_Id(void);
float Get_Current_Iq(void);

// Get DC Bus Voltage (Volts)
float Get_DC_Bus_Voltage(void);

#endif // CURRENT_SENSOR_H
