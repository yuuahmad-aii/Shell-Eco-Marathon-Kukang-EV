#include "current_sensor.h"
#include "six_step.h"
#include <math.h>

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;

static volatile float current_u = 0.0f;
static volatile float current_v = 0.0f;
static volatile float current_w = 0.0f;

// FOC Variables
static volatile float current_id = 0.0f;
static volatile float current_iq = 0.0f;
static volatile float current_id_filtered = 0.0f;
static volatile float current_iq_filtered = 0.0f;

#define LPF_ALPHA_CURRENT 0.05f

// Calibration variables
static volatile uint8_t is_calibrated = 0;
static volatile uint16_t calib_counter_u = 0;
static volatile uint16_t calib_counter_w = 0;
static volatile float sum_u_pin = 0.0f;
static volatile float sum_v_pin = 0.0f;
static volatile float sum_w_pin = 0.0f;

static float offset_u_pin = 0.0f;
static float offset_v_pin = 0.0f;
static float offset_w_pin = 0.0f;

// DC Bus Voltage (V)
static volatile float vbus_voltage_filtered = 0.0f;
#define LPF_ALPHA_VBUS 0.01f

// Helper function to convert ADC raw value to Voltage at the pin
static inline float ADC_To_Voltage(uint32_t adc_val) {
    return ((float)adc_val / ADC_MAX_VAL) * ADC_VREF;
}

// Helper function to convert Voltage at the pin to Amperes
static float Voltage_To_Amperes(float v_pin, float offset_pin) {
    // Subtract the calibrated idle offset voltage
    float v_pin_diff = v_pin - offset_pin;
    
    // Convert the pin voltage difference to the original ACS712 voltage difference
    float v_acs712_diff = v_pin_diff / VOLTAGE_DIVIDER_RATIO;
    
    // Convert ACS712 voltage difference to current
    return v_acs712_diff / ACS712_30A_SENSITIVITY;
}

void CurrentSensor_Init(void) {
    // Calibrate ADC1 and ADC2
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
    
    // Start Injected conversions with Interrupts
    HAL_ADCEx_InjectedStart_IT(&hadc1);
    HAL_ADCEx_InjectedStart_IT(&hadc2);

    // Wait until calibration is complete (blocking for ~100ms max at 20kHz PWM)
    while (!is_calibrated) {
        // Do nothing, just wait for interrupts to gather enough samples
    }
}

float Get_Current_U(void) {
    return current_u;
}

float Get_Current_V(void) {
    return current_v;
}

float Get_Current_W(void) {
    return current_w;
}

float Get_Current_Id(void) {
    return current_id_filtered;
}

float Get_Current_Iq(void) {
    return current_iq_filtered;
}

float Get_DC_Bus_Voltage(void) {
    return vbus_voltage_filtered;
}

// Interrupt callback called when Injected conversion is complete
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        uint32_t raw_u = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
        uint32_t raw_v = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_2);
        
        float v_u = ADC_To_Voltage(raw_u);
        float v_v = ADC_To_Voltage(raw_v);
        
        if (!is_calibrated) {
            if (calib_counter_u < CALIBRATION_SAMPLES) {
                sum_u_pin += v_u;
                sum_v_pin += v_v;
                calib_counter_u++;
            }
        } else {
            current_u = Voltage_To_Amperes(v_u, offset_u_pin);
            current_v = Voltage_To_Amperes(v_v, offset_v_pin);
        }
    } 
    else if (hadc->Instance == ADC2) {
        uint32_t raw_w = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_1);
        uint32_t raw_vbus = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_2);
        
        float v_w = ADC_To_Voltage(raw_w);
        float v_pin_vbus = ADC_To_Voltage(raw_vbus);
        float v_bus_measured = v_pin_vbus * VBUS_DIVIDER_RATIO;
        
        // Low Pass Filter for DC Bus Voltage
        if (vbus_voltage_filtered == 0.0f) {
            vbus_voltage_filtered = v_bus_measured;
        } else {
            vbus_voltage_filtered = (1.0f - LPF_ALPHA_VBUS) * vbus_voltage_filtered + (LPF_ALPHA_VBUS * v_bus_measured);
        }
        
        if (!is_calibrated) {
            if (calib_counter_w < CALIBRATION_SAMPLES) {
                sum_w_pin += v_w;
                calib_counter_w++;
            }
            // Check if both ADC1 and ADC2 have finished calibrating
            if (calib_counter_u >= CALIBRATION_SAMPLES && calib_counter_w >= CALIBRATION_SAMPLES) {
                offset_u_pin = sum_u_pin / (float)CALIBRATION_SAMPLES;
                offset_v_pin = sum_v_pin / (float)CALIBRATION_SAMPLES;
                offset_w_pin = sum_w_pin / (float)CALIBRATION_SAMPLES;
                is_calibrated = 1;
            }
        } else {
            current_w = Voltage_To_Amperes(v_w, offset_w_pin);
            
            // Perform FOC Transforms
            float theta = SixStep_GetElectricalAngle();
            
            // Clarke Transform
            float i_alpha = current_u;
            float i_beta = (current_u + 2.0f * current_v) * 0.577350269f; // 1/sqrt(3)
            
            // Park Transform
            current_id = i_alpha * cosf(theta) + i_beta * sinf(theta);
            current_iq = -i_alpha * sinf(theta) + i_beta * cosf(theta);
            
            // Low Pass Filter
            current_id_filtered = (1.0f - LPF_ALPHA_CURRENT) * current_id_filtered + LPF_ALPHA_CURRENT * current_id;
            current_iq_filtered = (1.0f - LPF_ALPHA_CURRENT) * current_iq_filtered + LPF_ALPHA_CURRENT * current_iq;
        }
    }
}
