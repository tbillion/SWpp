/**
 * @file esp32_adc.c
 * @brief ADC (Analog-to-Digital Converter) implementation for ESP32-S3
 * 
 * Implements 18-channel ADC with 12-bit to 16-bit scaling.
 * Uses ESP-IDF ADC driver with eFuse-based calibration.
 * 
 * @author AI Agent
 * @date 2026-01-28
 */

#include "esp32_adc.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <string.h>

static const char* TAG = "ESP32_ADC";

/**
 * @brief ADC module state
 */
static struct {
    bool initialized;
    ESP32_ADC_Config_t config;
    ESP32_ADC_Stats_t stats;
    esp_adc_cal_characteristics_t adc1_chars;
    esp_adc_cal_characteristics_t adc2_chars;
} adc_state = {
    .initialized = false
};

/**
 * @brief ADC channel mapping table
 * 
 * Maps Serial Wombat pins (0-17) to ESP32 GPIO and ADC channels.
 * Based on PIN_MAPPING.md Section 5.1.
 */
static const ESP32_ADC_ChannelInfo_t adc_channels[ESP32_ADC_TOTAL_PINS] = {
    // ADC1 Channels (10 channels: SW pins 0-8)
    {0,  1, ADC_UNIT_1, ADC_CHANNEL_0, true,  "Pin 0 (GPIO1, ADC1_CH0)"},
    {1,  2, ADC_UNIT_1, ADC_CHANNEL_1, true,  "Pin 1 (GPIO2, ADC1_CH1)"},
    {2,  4, ADC_UNIT_1, ADC_CHANNEL_3, true,  "Pin 2 (GPIO4, ADC1_CH3)"},
    {3,  5, ADC_UNIT_1, ADC_CHANNEL_4, true,  "Pin 3 (GPIO5, ADC1_CH4)"},
    {4,  6, ADC_UNIT_1, ADC_CHANNEL_5, true,  "Pin 4 (GPIO6, ADC1_CH5)"},
    {5,  7, ADC_UNIT_1, ADC_CHANNEL_6, true,  "Pin 5 (GPIO7, ADC1_CH6)"},
    {6,  8, ADC_UNIT_1, ADC_CHANNEL_7, true,  "Pin 6 (GPIO8, ADC1_CH7, I2C SDA)"},
    {7,  9, ADC_UNIT_1, ADC_CHANNEL_8, true,  "Pin 7 (GPIO9, ADC1_CH8, I2C SCL)"},
    {8,  10, ADC_UNIT_1, ADC_CHANNEL_9, true,  "Pin 8 (GPIO10, ADC1_CH9)"},
    
    // ADC2 Channels (8 channels: SW pins 9-16)
    {9,  11, ADC_UNIT_2, ADC_CHANNEL_0, true,  "Pin 9 (GPIO11, ADC2_CH0)"},
    {10, 12, ADC_UNIT_2, ADC_CHANNEL_1, true,  "Pin 10 (GPIO12, ADC2_CH1)"},
    {11, 13, ADC_UNIT_2, ADC_CHANNEL_2, true,  "Pin 11 (GPIO13, ADC2_CH2)"},
    {12, 14, ADC_UNIT_2, ADC_CHANNEL_3, true,  "Pin 12 (GPIO14, ADC2_CH3)"},
    {13, 15, ADC_UNIT_2, ADC_CHANNEL_4, true,  "Pin 13 (GPIO15, ADC2_CH4)"},
    {14, 16, ADC_UNIT_2, ADC_CHANNEL_5, true,  "Pin 14 (GPIO16, ADC2_CH5)"},
    {15, 17, ADC_UNIT_2, ADC_CHANNEL_6, true,  "Pin 15 (GPIO17, ADC2_CH6)"},
    {16, 18, ADC_UNIT_2, ADC_CHANNEL_7, true,  "Pin 16 (GPIO18, ADC2_CH7)"},
    
    // Digital-only pin (no ADC)
    {17, 21, ADC_UNIT_MAX, ADC_CHANNEL_MAX, false, "Pin 17 (GPIO21, Digital only)"}
};

/**
 * @brief Initialize ADC subsystem
 */
bool ESP32_ADC_Init(const ESP32_ADC_Config_t* config) {
    if (adc_state.initialized) {
        ESP_LOGW(TAG, "ADC already initialized");
        return true;
    }
    
    // Use provided config or defaults
    if (config != NULL) {
        memcpy(&adc_state.config, config, sizeof(ESP32_ADC_Config_t));
    } else {
        ESP32_ADC_Config_t default_config = ESP32_ADC_DEFAULT_CONFIG();
        memcpy(&adc_state.config, &default_config, sizeof(ESP32_ADC_Config_t));
    }
    
    ESP_LOGI(TAG, "Initializing ADC subsystem");
    ESP_LOGI(TAG, "  Attenuation: %d dB", adc_state.config.attenuation * 2.5);
    ESP_LOGI(TAG, "  Width: %d bits", 9 + adc_state.config.width);
    ESP_LOGI(TAG, "  Samples: %d", adc_state.config.samples);
    ESP_LOGI(TAG, "  Calibration: %s", adc_state.config.use_calibration ? "Enabled" : "Disabled");
    
    // Configure ADC1 width (global setting for ADC1)
    esp_err_t ret = adc1_config_width(adc_state.config.width);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC1 width: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Configure ADC1 channels (pins 0-8)
    for (int i = 0; i < 9; i++) {
        if (adc_channels[i].available && adc_channels[i].adc_unit == ADC_UNIT_1) {
            ret = adc1_config_channel_atten(adc_channels[i].adc_channel, adc_state.config.attenuation);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to configure ADC1 channel %d: %s", 
                         adc_channels[i].adc_channel, esp_err_to_name(ret));
                return false;
            }
            ESP_LOGD(TAG, "Configured ADC1 channel %d (Pin %d, GPIO %d)", 
                     adc_channels[i].adc_channel, i, adc_channels[i].gpio_num);
        }
    }
    
    // Configure ADC2 channels (pins 9-16)
    for (int i = 9; i < 17; i++) {
        if (adc_channels[i].available && adc_channels[i].adc_unit == ADC_UNIT_2) {
            ret = adc2_config_channel_atten(adc_channels[i].adc_channel, adc_state.config.attenuation);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to configure ADC2 channel %d: %s", 
                         adc_channels[i].adc_channel, esp_err_to_name(ret));
                return false;
            }
            ESP_LOGD(TAG, "Configured ADC2 channel %d (Pin %d, GPIO %d)", 
                     adc_channels[i].adc_channel, i, adc_channels[i].gpio_num);
        }
    }
    
    // Initialize calibration if requested
    if (adc_state.config.use_calibration) {
        esp_adc_cal_value_t val_type;
        
        // Calibrate ADC1
        val_type = esp_adc_cal_characterize(ADC_UNIT_1, adc_state.config.attenuation, 
                                            adc_state.config.width, 1100, &adc_state.adc1_chars);
        if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
            ESP_LOGI(TAG, "ADC1 calibration: eFuse Two Point");
        } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
            ESP_LOGI(TAG, "ADC1 calibration: eFuse Vref");
        } else {
            ESP_LOGW(TAG, "ADC1 calibration: Default (no eFuse data)");
        }
        
        // Calibrate ADC2
        val_type = esp_adc_cal_characterize(ADC_UNIT_2, adc_state.config.attenuation, 
                                            adc_state.config.width, 1100, &adc_state.adc2_chars);
        if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
            ESP_LOGI(TAG, "ADC2 calibration: eFuse Two Point");
        } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
            ESP_LOGI(TAG, "ADC2 calibration: eFuse Vref");
        } else {
            ESP_LOGW(TAG, "ADC2 calibration: Default (no eFuse data)");
            ESP_LOGW(TAG, "ADC2 may conflict with WiFi when enabled (Phase 2+)");
        }
        
        adc_state.stats.calibration_valid = true;
    } else {
        adc_state.stats.calibration_valid = false;
    }
    
    // Reset statistics
    adc_state.stats.reads_adc1 = 0;
    adc_state.stats.reads_adc2 = 0;
    adc_state.stats.errors_adc1 = 0;
    adc_state.stats.errors_adc2 = 0;
    adc_state.stats.overruns = 0;
    adc_state.stats.underruns = 0;
    
    adc_state.initialized = true;
    ESP_LOGI(TAG, "ADC subsystem initialized successfully");
    ESP_LOGI(TAG, "  ADC1: 10 channels (pins 0-8)");
    ESP_LOGI(TAG, "  ADC2: 8 channels (pins 9-16, WiFi conflict warning)");
    ESP_LOGI(TAG, "  Pin 17 (GPIO21): Digital only, no ADC");
    
    return true;
}

/**
 * @brief Deinitialize ADC subsystem
 */
void ESP32_ADC_Deinit(void) {
    if (!adc_state.initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing ADC subsystem");
    adc_state.initialized = false;
}

/**
 * @brief Check if ADC is initialized
 */
bool ESP32_ADC_IsInitialized(void) {
    return adc_state.initialized;
}

/**
 * @brief Read raw 12-bit ADC value with averaging
 */
uint16_t ESP32_ADC_ReadRaw(uint8_t sw_pin) {
    if (!adc_state.initialized) {
        ESP_LOGE(TAG, "ADC not initialized");
        return 0;
    }
    
    if (sw_pin >= ESP32_ADC_TOTAL_PINS) {
        ESP_LOGE(TAG, "Invalid pin %d", sw_pin);
        return 0;
    }
    
    const ESP32_ADC_ChannelInfo_t* info = &adc_channels[sw_pin];
    if (!info->available) {
        ESP_LOGW(TAG, "Pin %d has no ADC (GPIO %d)", sw_pin, info->gpio_num);
        return 0;
    }
    
    uint32_t sum = 0;
    uint8_t valid_samples = 0;
    
    // Multi-sample averaging
    for (uint8_t i = 0; i < adc_state.config.samples; i++) {
        int raw = 0;
        
        if (info->adc_unit == ADC_UNIT_1) {
            raw = adc1_get_raw(info->adc_channel);
            if (raw >= 0) {
                sum += raw;
                valid_samples++;
            } else {
                adc_state.stats.errors_adc1++;
            }
        } else if (info->adc_unit == ADC_UNIT_2) {
            int reading;
            esp_err_t ret = adc2_get_raw(info->adc_channel, adc_state.config.width, &reading);
            if (ret == ESP_OK) {
                sum += reading;
                valid_samples++;
            } else {
                adc_state.stats.errors_adc2++;
                if (ret == ESP_ERR_INVALID_STATE) {
                    ESP_LOGW(TAG, "ADC2 channel %d unavailable (WiFi conflict?)", info->adc_channel);
                }
            }
        }
    }
    
    if (valid_samples == 0) {
        ESP_LOGE(TAG, "All ADC reads failed for pin %d", sw_pin);
        return 0;
    }
    
    uint16_t avg = sum / valid_samples;
    
    // Update statistics
    if (info->adc_unit == ADC_UNIT_1) {
        adc_state.stats.reads_adc1++;
    } else {
        adc_state.stats.reads_adc2++;
    }
    
    // Check for overruns/underruns
    if (avg > 4095) {
        adc_state.stats.overruns++;
        avg = 4095;
    }
    
    return avg;
}

/**
 * @brief Read 16-bit scaled ADC value
 * 
 * Scales 12-bit (0-4095) to 16-bit (0-65535) for Serial Wombat protocol.
 */
uint16_t ESP32_ADC_Read16Bit(uint8_t sw_pin) {
    uint16_t raw = ESP32_ADC_ReadRaw(sw_pin);
    // Scale: 12-bit to 16-bit (multiply by 16, or shift left 4)
    return raw << 4;
}

/**
 * @brief Read calibrated voltage in millivolts
 */
uint32_t ESP32_ADC_ReadMillivolts(uint8_t sw_pin) {
    if (!adc_state.initialized || !adc_state.config.use_calibration) {
        return 0;
    }
    
    if (sw_pin >= ESP32_ADC_TOTAL_PINS) {
        return 0;
    }
    
    const ESP32_ADC_ChannelInfo_t* info = &adc_channels[sw_pin];
    if (!info->available) {
        return 0;
    }
    
    uint16_t raw = ESP32_ADC_ReadRaw(sw_pin);
    if (raw == 0) {
        return 0;
    }
    
    uint32_t voltage = 0;
    if (info->adc_unit == ADC_UNIT_1) {
        voltage = esp_adc_cal_raw_to_voltage(raw, &adc_state.adc1_chars);
    } else if (info->adc_unit == ADC_UNIT_2) {
        voltage = esp_adc_cal_raw_to_voltage(raw, &adc_state.adc2_chars);
    }
    
    return voltage;
}

/**
 * @brief Read calibrated voltage as float
 */
float ESP32_ADC_ReadVolts(uint8_t sw_pin) {
    uint32_t mv = ESP32_ADC_ReadMillivolts(sw_pin);
    return mv / 1000.0f;
}

/**
 * @brief Read multiple ADC channels
 */
uint8_t ESP32_ADC_ReadMultiple(const uint8_t* sw_pins, uint16_t* values, uint8_t count) {
    if (!adc_state.initialized || sw_pins == NULL || values == NULL) {
        return 0;
    }
    
    uint8_t successful = 0;
    for (uint8_t i = 0; i < count; i++) {
        values[i] = ESP32_ADC_Read16Bit(sw_pins[i]);
        if (values[i] > 0 || (sw_pins[i] < ESP32_ADC_TOTAL_PINS && adc_channels[sw_pins[i]].available)) {
            successful++;
        }
    }
    
    return successful;
}

/**
 * @brief Get ADC channel information
 */
const ESP32_ADC_ChannelInfo_t* ESP32_ADC_GetChannelInfo(uint8_t sw_pin) {
    if (sw_pin >= ESP32_ADC_TOTAL_PINS) {
        return NULL;
    }
    return &adc_channels[sw_pin];
}

/**
 * @brief Check if pin has ADC capability
 */
bool ESP32_ADC_IsAvailable(uint8_t sw_pin) {
    if (sw_pin >= ESP32_ADC_TOTAL_PINS) {
        return false;
    }
    return adc_channels[sw_pin].available;
}

/**
 * @brief Get ADC unit for a pin
 */
adc_unit_t ESP32_ADC_GetUnit(uint8_t sw_pin) {
    if (sw_pin >= ESP32_ADC_TOTAL_PINS) {
        return ADC_UNIT_MAX;
    }
    return adc_channels[sw_pin].adc_unit;
}

/**
 * @brief Get ADC channel for a pin
 */
adc_channel_t ESP32_ADC_GetChannel(uint8_t sw_pin) {
    if (sw_pin >= ESP32_ADC_TOTAL_PINS) {
        return ADC_CHANNEL_MAX;
    }
    return adc_channels[sw_pin].adc_channel;
}

/**
 * @brief Get ADC statistics
 */
void ESP32_ADC_GetStats(ESP32_ADC_Stats_t* stats) {
    if (stats != NULL) {
        memcpy(stats, &adc_state.stats, sizeof(ESP32_ADC_Stats_t));
    }
}

/**
 * @brief Reset ADC statistics
 */
void ESP32_ADC_ResetStats(void) {
    adc_state.stats.reads_adc1 = 0;
    adc_state.stats.reads_adc2 = 0;
    adc_state.stats.errors_adc1 = 0;
    adc_state.stats.errors_adc2 = 0;
    adc_state.stats.overruns = 0;
    adc_state.stats.underruns = 0;
}

/**
 * @brief Set number of samples for averaging
 */
bool ESP32_ADC_SetSamples(uint8_t samples) {
    if (samples < 1 || samples > 16) {
        ESP_LOGE(TAG, "Invalid sample count %d (must be 1-16)", samples);
        return false;
    }
    
    adc_state.config.samples = samples;
    ESP_LOGI(TAG, "ADC samples set to %d", samples);
    return true;
}

/**
 * @brief Get current sample count
 */
uint8_t ESP32_ADC_GetSamples(void) {
    return adc_state.config.samples;
}

/**
 * @brief Recalibrate ADC
 */
bool ESP32_ADC_Recalibrate(void) {
    if (!adc_state.initialized) {
        return false;
    }
    
    ESP_LOGI(TAG, "Recalibrating ADC...");
    
    esp_adc_cal_value_t val_type;
    
    val_type = esp_adc_cal_characterize(ADC_UNIT_1, adc_state.config.attenuation, 
                                        adc_state.config.width, 1100, &adc_state.adc1_chars);
    val_type = esp_adc_cal_characterize(ADC_UNIT_2, adc_state.config.attenuation, 
                                        adc_state.config.width, 1100, &adc_state.adc2_chars);
    
    adc_state.stats.calibration_valid = true;
    ESP_LOGI(TAG, "ADC recalibration complete");
    
    return true;
}

/**
 * @brief Check if ADC2 will conflict with WiFi
 * 
 * In Phase 1, WiFi is not active, so always returns false.
 * In Phase 2+, this should check WiFi status.
 */
bool ESP32_ADC_IsADC2Conflicted(void) {
    // Phase 1: WiFi not implemented yet
    return false;
}
