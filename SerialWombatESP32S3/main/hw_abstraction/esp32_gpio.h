/**
 * @file esp32_gpio.h
 * @brief ESP32-S3 GPIO Hardware Abstraction Layer for Serial Wombat
 * 
 * Copyright 2020-2026 Broadwell Consulting Inc.
 * 
 * This file provides the GPIO abstraction for Serial Wombat on ESP32-S3.
 * It implements the pin mapping strategy defined in PIN_MAPPING.md.
 * 
 * Key Features:
 * - 22 safe GPIOs (18 legacy + 4 extended)
 * - Pin safety validation (no strapping/flash/USB pins)
 * - ADC capability tracking (18 ADC-capable pins)
 * - Pull-up/pull-down configuration
 * - Open-drain support
 * 
 * Governed by: PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md Section 3
 */

#ifndef ESP32_GPIO_H
#define ESP32_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Total number of Serial Wombat pins (from PIN_MAPPING.md)
#define SW_PIN_COUNT 22
#define SW_PIN_COUNT_LEGACY 18  // Pins 0-17 (SW18AB compatibility)
#define SW_PIN_COUNT_EXTENDED 4  // Pins 18-21 (ESP32-specific)

// Pin capability flags
#define PIN_CAP_DIGITAL   (1 << 0)  // Digital I/O capability
#define PIN_CAP_ADC       (1 << 1)  // ADC capability
#define PIN_CAP_ADC1      (1 << 2)  // ADC1 channel (no WiFi conflict)
#define PIN_CAP_ADC2      (1 << 3)  // ADC2 channel (WiFi conflict)
#define PIN_CAP_PWM       (1 << 4)  // PWM capability (LEDC)
#define PIN_CAP_I2C       (1 << 5)  // I2C capable
#define PIN_CAP_JTAG      (1 << 6)  // JTAG pin (warning if used)

/**
 * @brief Pin mapping structure
 * Maps Serial Wombat pin numbers to ESP32 GPIO numbers
 */
typedef struct {
    uint8_t sw_pin;        // Serial Wombat pin number (0-21)
    uint8_t gpio_num;      // ESP32 GPIO number
    uint8_t adc_channel;   // ADC channel number (0xFF if not ADC-capable)
    uint8_t adc_unit;      // ADC unit (1 or 2, 0 if not ADC-capable)
    uint16_t capabilities; // Pin capability flags
    const char* name;      // Human-readable name
} sw_pin_map_t;

/**
 * @brief Initialize GPIO subsystem
 * Sets up the GPIO HAL and validates pin configuration
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_GPIO_Init(void);

/**
 * @brief Get ESP32 GPIO number for a Serial Wombat pin
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @return ESP32 GPIO number, or GPIO_NUM_NC if invalid
 */
gpio_num_t ESP32_GPIO_GetGPIONum(uint8_t sw_pin);

/**
 * @brief Get pin capabilities
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @return Pin capability flags, or 0 if invalid
 */
uint16_t ESP32_GPIO_GetCapabilities(uint8_t sw_pin);

/**
 * @brief Check if pin has a specific capability
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @param capability Capability flag to check
 * @return true if pin has capability, false otherwise
 */
bool ESP32_GPIO_HasCapability(uint8_t sw_pin, uint16_t capability);

/**
 * @brief Set pin mode (input/output)
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @param mode GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, GPIO_MODE_INPUT_OUTPUT
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_GPIO_SetMode(uint8_t sw_pin, gpio_mode_t mode);

/**
 * @brief Set pin output high
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_GPIO_SetHigh(uint8_t sw_pin);

/**
 * @brief Set pin output low
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_GPIO_SetLow(uint8_t sw_pin);

/**
 * @brief Set pin output level
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @param level 0 for low, non-zero for high
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_GPIO_SetLevel(uint8_t sw_pin, uint8_t level);

/**
 * @brief Read pin input level
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @return 1 if high, 0 if low, -1 on error
 */
int ESP32_GPIO_Read(uint8_t sw_pin);

/**
 * @brief Configure pull-up resistor
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @param enable true to enable pull-up, false to disable
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_GPIO_SetPullUp(uint8_t sw_pin, bool enable);

/**
 * @brief Configure pull-down resistor
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @param enable true to enable pull-down, false to disable
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_GPIO_SetPullDown(uint8_t sw_pin, bool enable);

/**
 * @brief Configure open-drain mode
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @param enable true to enable open-drain, false for push-pull
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_GPIO_SetOpenDrain(uint8_t sw_pin, bool enable);

/**
 * @brief Get ADC channel information for a pin
 * 
 * @param sw_pin Serial Wombat pin number (0-21)
 * @param[out] adc_unit ADC unit (1 or 2), set to 0 if not ADC-capable
 * @param[out] adc_channel ADC channel number
 * @return ESP_OK if ADC-capable, ESP_FAIL if not
 */
esp_err_t ESP32_GPIO_GetADCInfo(uint8_t sw_pin, uint8_t* adc_unit, uint8_t* adc_channel);

/**
 * @brief Get pin mapping table (for debugging/diagnostics)
 * 
 * @return Pointer to pin mapping table
 */
const sw_pin_map_t* ESP32_GPIO_GetPinMap(void);

#ifdef __cplusplus
}
#endif

#endif // ESP32_GPIO_H
