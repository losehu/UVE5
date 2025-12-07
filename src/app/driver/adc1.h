/* ESP32 ADC Driver for Battery Monitoring
 * Ported from UV-K5 firmware
 * 
 * Hardware Configuration:
 * - ADC1_CH8 (GPIO 9): Battery Current measurement
 * - ADC1_CH9 (GPIO 10): Battery Voltage measurement
 */

#ifndef DRIVER_ADC_ESP32_H
#define DRIVER_ADC_ESP32_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ADC Channel Definitions (matching original enum structure)
enum ADC_CH_MASK {
    ADC_CH0 = 0x0001U,  // GPIO 9  - Battery Current (ADC1_CH8)
    ADC_CH1 = 0x0002U,  // GPIO 10 - Battery Voltage (ADC1_CH9)
    ADC_CH2 = 0x0004U,
    ADC_CH3 = 0x0008U,
    ADC_CH4 = 0x0010U,
    ADC_CH5 = 0x0020U,
    ADC_CH6 = 0x0040U,
    ADC_CH7 = 0x0080U,
    ADC_CH8 = 0x0100U,
    ADC_CH9 = 0x0200U,
    ADC_CH10 = 0x0400U,
    ADC_CH11 = 0x0800U,
    ADC_CH12 = 0x1000U,
    ADC_CH13 = 0x2000U,
    ADC_CH14 = 0x4000U,
    ADC_CH15 = 0x8000U,
};

typedef enum ADC_CH_MASK ADC_CH_MASK;

// GPIO Pin Definitions for ADC channels
#define ADC_GPIO_CURRENT    9   // GPIO 9  - ADC1_CH8 - Battery Current
#define ADC_GPIO_VOLTAGE    10  // GPIO 10 - ADC1_CH9 - Battery Voltage

// Function Prototypes (matching original API)
uint8_t ADC_GetChannelNumber(ADC_CH_MASK Mask);
void ADC_Disable(void);
void ADC_Enable(void);
void ADC_SoftReset(void);
void ADC_Configure(void);
void ADC_Start(void);
bool ADC_CheckEndOfConversion(ADC_CH_MASK Mask);
uint16_t ADC_GetValue(ADC_CH_MASK Mask);

// ESP32-specific helper functions
void BOARD_ADC_GetBatteryInfo(uint16_t *pVoltage, uint16_t *pCurrent);

#ifdef __cplusplus
}
#endif

#endif // DRIVER_ADC_ESP32_H
