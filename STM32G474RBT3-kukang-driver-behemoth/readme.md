# STM32G474 Motor Controller (Behemoth)

This repository contains the firmware and hardware pinout documentation for a custom motor controller built around the **STM32G474** microcontroller.

## Hardware Specifications

*   **Microcontroller:** STM32G474RBT3
*   **Gate Driver:** DRV8353SR
*   **MOSFETs:** IPT015N10N5
*   **Current Sensing:** 3x Low-Side Shunt Resistors

## Pinout Mapping

Based on the schematic, here is the pin configuration for the STM32G474:

### Motor Control & Gate Driver (DRV8353SR)
*   **PA8:** DRIVER_HIGH_C
*   **PA9:** DRIVER_HIGH_B
*   **PA10:** DRIVER_HIGH_A
*   **PB13:** DRIVER_LOW_C
*   **PB14:** DRIVER_LOW_B
*   **PB15:** DRIVER_LOW_A
*   **PC8:** STM32_EN_GATE
*   **PC9:** STM32_DC_CAL

### Gate Driver SPI (DRV8353SR)
*   **PA15:** SPI_DRIVER_CS
*   **PC10:** SPI_DRIVER_CLK
*   **PC11:** SPI_DRIVER_MISO
*   **PC12:** SPI_DRIVER_MOSI

### Current Sensing (3-Shunt Low-Side)
*   **PB0:** STM32_CUR_C
*   **PB1:** STM32_CUR_B
*   **PB2:** STM32_CUR_A

### Voltage Sensing
*   **PC0:** VOLTAGE_SENSE_C
*   **PC1:** VOLTAGE_SENSE_B
*   **PC2:** VOLTAGE_SENSE_A
*   **PC3:** VOLTAGE_SENSE (Bus/Total)

### Hall Sensors
*   **PA0:** STM32_HALL_U
*   **PA1:** STM32_HALL_V
*   **PA2:** STM32_HALL_W

### Encoder (SPI)
*   **PA4:** SPI_ENC_CS
*   **PA5:** SPI_ENC_CLK
*   **PA6:** SPI_ENC_MISO
*   **PA7:** SPI_ENC_MOSI

### Encoder (Quadrature)
*   **PB3:** STM32_ENC_Z
*   **PB4:** STM32_ENC_B
*   **PB5:** STM32_ENC_A

### Communications
*   **PA11:** USB-
*   **PA12:** USB+
*   **PB8 (BOOT0):** STM32_CAN_RX
*   **PB9:** STM32_CAN_TX
*   **PB6:** STM32_MODBUS_TX
*   **PB7:** STM32_MODBUS_RX
*   **PD2:** STM32_MODBUS_SEL

### System & Debug
*   **PG10:** STM32_NRST
*   **PA13:** STM32_SWDIO
*   **PA14:** STM32_SWCLK
*   **PB10:** LED2 (Active High, 4.7k res)

---
*Note: The main program (`Core/Src/main.c`) initializes the necessary peripherals including ADCs (ADC1, ADC2, ADC3) for voltage/current sensing, Timers (TIM1 for PWM, TIM2 for Hall/Encoder), SPI (SPI3 for DRV8353SR/Encoder), and USB.*
