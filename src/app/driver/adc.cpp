/* ESP32 ADC Driver for Battery Monitoring
 * Ported from UV-K5 firmware
 * 
 * Hardware Configuration:
 * - ADC1_CH8 (GPIO 9): Battery Current measurement
 * - ADC1_CH9 (GPIO 10): Battery Voltage measurement
 */

#include "adc1.h"
#include <Arduino.h>

static bool adc_initialized = false;

// Last read values for CheckEndOfConversion simulation
static uint16_t last_voltage_value = 0;
static uint16_t last_current_value = 0;
static bool conversion_done = false;

/**
 * Get channel number from mask (bit position)
 * Compatible with original UV-K5 API
 */
uint8_t ADC_GetChannelNumber(ADC_CH_MASK Mask) {
    // Return the bit position (0-15)
    uint8_t channel = 0;
    uint16_t mask = Mask;
    while (mask > 1) {
        mask >>= 1;
        channel++;
    }
    return channel;
}

/**
 * Configure ADC channels
 * Matches original ADC_Configure() API
 */
void ADC_Configure(void) {
    if (adc_initialized) {
        return;
    }

    // Set ADC resolution to 12-bit (0-4095)
    analogReadResolution(12);
    
    // Configure GPIO pins for analog input
    // Note: analogRead() automatically configures the pin, but we can explicitly set it to INPUT
    pinMode(ADC_GPIO_CURRENT, INPUT);   // GPIO 9 - Battery Current
    pinMode(ADC_GPIO_VOLTAGE, INPUT);   // GPIO 10 - Battery Voltage

    adc_initialized = true;
    conversion_done = false;
}

/**
 * Disable ADC (cleanup)
 */
void ADC_Disable(void) {
    adc_initialized = false;
}

/**
 * Enable ADC
 */
void ADC_Enable(void) {
    if (!adc_initialized) {
        ADC_Configure();
    }
}

/**
 * Soft reset ADC (not needed for ESP32, but kept for API compatibility)
 */
void ADC_SoftReset(void) {
    // ESP32 ADC doesn't need soft reset
    // Keep for API compatibility
}

/**
 * Start ADC conversion
 * In ESP32, conversion happens immediately on read
 */
void ADC_Start(void) {
    if (!adc_initialized) {
        ADC_Configure();
    }
    conversion_done = false;
    
    // Pre-read both channels to simulate conversion start
    last_voltage_value = (uint16_t)analogRead(ADC_GPIO_VOLTAGE);  // GPIO 10
    last_current_value = (uint16_t)analogRead(ADC_GPIO_CURRENT);  // GPIO 9
    
    conversion_done = true;
}

/**
 * Check if conversion is complete
 * ESP32 ADC is blocking, so always return true after Start
 */
bool ADC_CheckEndOfConversion(ADC_CH_MASK Mask) {
    (void)Mask;  // Unused in ESP32 implementation
    return conversion_done;
}

/**
 * Get ADC raw value (12-bit, 0-4095)
 * Compatible with original UV-K5 API
 */
uint16_t ADC_GetValue(ADC_CH_MASK Mask) {
    if (!adc_initialized) {
        ADC_Configure();
    }

    int adc_raw = 0;
    
    if (Mask == ADC_CH0) {
        // Battery Current - GPIO 9
        adc_raw = analogRead(ADC_GPIO_CURRENT);
        last_current_value = (uint16_t)adc_raw;
    } else if (Mask == ADC_CH1) {
        // Battery Voltage - GPIO 10
        adc_raw = analogRead(ADC_GPIO_VOLTAGE);
        last_voltage_value = (uint16_t)adc_raw;
    } else {
        // Other channels not implemented
        return 0;
    }

    // Return 12-bit raw value (0-4095)
    return (uint16_t)(adc_raw & 0xFFF);
}

/**
 * Get battery voltage and current
 * Compatible with BOARD_ADC_GetBatteryInfo from original firmware
 * 
 * @param pVoltage Pointer to store voltage ADC value (0-4095)
 * @param pCurrent Pointer to store current ADC value (0-4095)
 */
void BOARD_ADC_GetBatteryInfo(uint16_t *pVoltage, uint16_t *pCurrent) {
    ADC_Start();
    while (!ADC_CheckEndOfConversion(ADC_CH9)) {}
    *pVoltage = ADC_GetValue(ADC_CH4);
    *pCurrent = ADC_GetValue(ADC_CH9);
}
