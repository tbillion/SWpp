/**
 * @file esp32_adc.h
 * @brief ADC (Analog-to-Digital Converter) abstraction for ESP32-S3
 * 
 * Provides hardware abstraction for ADC functionality with 18 channels
 * across ADC1 (10 channels) and ADC2 (8 channels). Implements 12-bit to
 * 16-bit scaling for Serial Wombat protocol compatibility.
 * 
 * Contract Compliance:
 * - Section 3.3: Feature availability (ADC on capable pins only)
 * - PIN_MAPPING.md Section 7.1: ADC channel assignments
 * - DEVIATIONS_FROM_SW18AB.md Dev-1: 12-bit vs 10-bit resolution
 * 
 * @author AI Agent
 * @date 2026-01-28
 */

#ifndef ESP32_ADC_H
#define ESP32_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ADC configuration structure
 */
typedef struct {
    adc_atten_t attenuation;     // ADC attenuation (0dB, 2.5dB, 6dB, 11dB)
    adc_bits_width_t width;      // ADC resolution (9-12 bits)
    uint8_t samples;             // Number of samples to average (1-16)
    bool use_calibration;        // Use eFuse calibration
} ESP32_ADC_Config_t;

/**
 * @brief ADC channel information
 */
typedef struct {
    uint8_t sw_pin;              // Serial Wombat pin number (0-17)
    uint8_t gpio_num;            // ESP32 GPIO number
    adc_unit_t adc_unit;         // ADC unit (ADC_UNIT_1 or ADC_UNIT_2)
    adc_channel_t adc_channel;   // ADC channel number
    bool available;              // Channel is available (not digital-only)
    const char* name;            // Human-readable name
} ESP32_ADC_ChannelInfo_t;

/**
 * @brief ADC statistics
 */
typedef struct {
    uint32_t reads_adc1;         // Number of ADC1 reads
    uint32_t reads_adc2;         // Number of ADC2 reads
    uint32_t errors_adc1;        // ADC1 read errors
    uint32_t errors_adc2;        // ADC2 read errors
    uint32_t overruns;           // Value overruns (>4095)
    uint32_t underruns;          // Value underruns (<0, shouldn't happen)
    bool calibration_valid;      // Calibration was successful
} ESP32_ADC_Stats_t;

/**
 * @brief Default ADC configuration
 * - Attenuation: 11dB (0-3.3V range)
 * - Width: 12-bit (0-4095)
 * - Samples: 4 (averaging for stability)
 * - Calibration: Enabled
 */
#define ESP32_ADC_DEFAULT_CONFIG() { \
    .attenuation = ADC_ATTEN_DB_11, \
    .width = ADC_WIDTH_BIT_12, \
    .samples = 4, \
    .use_calibration = true \
}

/**
 * @brief Total number of Serial Wombat pins
 */
#define ESP32_ADC_TOTAL_PINS 18

/**
 * @brief Number of ADC-capable pins (excludes pin 17 which has no ADC)
 */
#define ESP32_ADC_NUM_CHANNELS 18

/**
 * @brief Number of ADC1 channels
 */
#define ESP32_ADC_NUM_ADC1_CHANNELS 10

/**
 * @brief Number of ADC2 channels
 */
#define ESP32_ADC_NUM_ADC2_CHANNELS 8

/**
 * @brief Initialize ADC subsystem
 * 
 * Configures both ADC1 and ADC2 with specified settings.
 * Sets up eFuse-based calibration if requested.
 * 
 * @param config ADC configuration (NULL = use defaults)
 * @return true if initialization succeeded
 */
bool ESP32_ADC_Init(const ESP32_ADC_Config_t* config);

/**
 * @brief Deinitialize ADC subsystem
 */
void ESP32_ADC_Deinit(void);

/**
 * @brief Check if ADC is initialized
 * 
 * @return true if ADC subsystem is ready
 */
bool ESP32_ADC_IsInitialized(void);

/**
 * @brief Read raw 12-bit ADC value
 * 
 * Reads the ADC channel with optional multi-sample averaging.
 * 
 * @param sw_pin Serial Wombat pin number (0-17)
 * @return Raw 12-bit value (0-4095), or 0 on error
 */
uint16_t ESP32_ADC_ReadRaw(uint8_t sw_pin);

/**
 * @brief Read 16-bit scaled ADC value
 * 
 * Reads ADC and scales from 12-bit (0-4095) to 16-bit (0-65535)
 * for Serial Wombat protocol compatibility.
 * 
 * Scaling formula: value16 = value12 * 16 (or << 4)
 * 
 * @param sw_pin Serial Wombat pin number (0-17)
 * @return 16-bit scaled value (0-65535), or 0 on error
 */
uint16_t ESP32_ADC_Read16Bit(uint8_t sw_pin);

/**
 * @brief Read calibrated voltage in millivolts
 * 
 * Uses eFuse calibration data to convert ADC reading to voltage.
 * 
 * @param sw_pin Serial Wombat pin number (0-17)
 * @return Voltage in millivolts (0-3300), or 0 on error
 */
uint32_t ESP32_ADC_ReadMillivolts(uint8_t sw_pin);

/**
 * @brief Read calibrated voltage as float
 * 
 * Uses eFuse calibration data to convert ADC reading to voltage.
 * 
 * @param sw_pin Serial Wombat pin number (0-17)
 * @return Voltage as float (0.0-3.3), or 0.0 on error
 */
float ESP32_ADC_ReadVolts(uint8_t sw_pin);

/**
 * @brief Read multiple ADC channels at once
 * 
 * Efficient bulk reading of ADC values.
 * 
 * @param sw_pins Array of Serial Wombat pin numbers
 * @param values Array to store 16-bit scaled values
 * @param count Number of pins to read
 * @return Number of successful reads
 */
uint8_t ESP32_ADC_ReadMultiple(const uint8_t* sw_pins, uint16_t* values, uint8_t count);

/**
 * @brief Get ADC channel information
 * 
 * @param sw_pin Serial Wombat pin number (0-17)
 * @return Pointer to channel info, or NULL if invalid
 */
const ESP32_ADC_ChannelInfo_t* ESP32_ADC_GetChannelInfo(uint8_t sw_pin);

/**
 * @brief Check if pin has ADC capability
 * 
 * @param sw_pin Serial Wombat pin number (0-17)
 * @return true if pin supports ADC
 */
bool ESP32_ADC_IsAvailable(uint8_t sw_pin);

/**
 * @brief Get ADC unit for a pin
 * 
 * @param sw_pin Serial Wombat pin number (0-17)
 * @return ADC_UNIT_1, ADC_UNIT_2, or ADC_UNIT_MAX if not available
 */
adc_unit_t ESP32_ADC_GetUnit(uint8_t sw_pin);

/**
 * @brief Get ADC channel for a pin
 * 
 * @param sw_pin Serial Wombat pin number (0-17)
 * @return ADC channel, or ADC_CHANNEL_MAX if not available
 */
adc_channel_t ESP32_ADC_GetChannel(uint8_t sw_pin);

/**
 * @brief Get ADC statistics
 * 
 * @param stats Pointer to statistics structure
 */
void ESP32_ADC_GetStats(ESP32_ADC_Stats_t* stats);

/**
 * @brief Reset ADC statistics
 */
void ESP32_ADC_ResetStats(void);

/**
 * @brief Set number of samples for averaging
 * 
 * More samples = more stable, but slower.
 * 
 * @param samples Number of samples (1-16)
 * @return true if set successfully
 */
bool ESP32_ADC_SetSamples(uint8_t samples);

/**
 * @brief Get current sample count
 * 
 * @return Number of samples being averaged
 */
uint8_t ESP32_ADC_GetSamples(void);

/**
 * @brief Recalibrate ADC using eFuse data
 * 
 * Can be called to refresh calibration at runtime.
 * 
 * @return true if calibration succeeded
 */
bool ESP32_ADC_Recalibrate(void);

/**
 * @brief Check if ADC2 will conflict with WiFi
 * 
 * ADC2 is shared with WiFi radio. When WiFi is active (Phase 2+),
 * ADC2 channels may not be available or may give incorrect readings.
 * 
 * @return true if WiFi is active and ADC2 may be unreliable
 */
bool ESP32_ADC_IsADC2Conflicted(void);

#ifdef __cplusplus
}
#endif

#endif // ESP32_ADC_H
