/**
 * @file esp32_gpio.c
 * @brief ESP32-S3 GPIO Hardware Abstraction Layer Implementation
 * 
 * Copyright 2020-2026 Broadwell Consulting Inc.
 * 
 * Pin mapping based on PIN_MAPPING.md:
 * - 22 safe GPIOs total
 * - 18 legacy pins (0-17) for SW18AB compatibility
 * - 4 extended pins (18-21) for ESP32-specific features
 * - 18 ADC-capable pins (10 on ADC1, 8 on ADC2)
 * 
 * Safety notes:
 * - GPIO 0, 3, 45, 46: Strapping pins (EXCLUDED)
 * - GPIO 26-37: Flash/PSRAM pins on N16R8 (EXCLUDED)
 * - GPIO 19-20: USB pins (EXCLUDED in Phase 1)
 * - GPIO 39-42: JTAG pins (INCLUDED with warning)
 */

#include "esp32_gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>

static const char* TAG = "SW_GPIO";

/**
 * Pin mapping table from PIN_MAPPING.md Section 5.1
 * 
 * This table maps Serial Wombat pin numbers to ESP32-S3 GPIO numbers.
 * The mapping is designed for boot safety and maximum feature coverage.
 */
static const sw_pin_map_t pin_map[SW_PIN_COUNT] = {
    // Legacy Pins (0-17) - SW18AB Compatibility
    {0,  GPIO_NUM_1,  0, 1, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC1 | PIN_CAP_PWM, "SW0/GPIO1/ADC1_CH0"},
    {1,  GPIO_NUM_2,  1, 1, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC1 | PIN_CAP_PWM, "SW1/GPIO2/ADC1_CH1"},
    {2,  GPIO_NUM_4,  3, 1, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC1 | PIN_CAP_PWM, "SW2/GPIO4/ADC1_CH3"},
    {3,  GPIO_NUM_5,  4, 1, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC1 | PIN_CAP_PWM, "SW3/GPIO5/ADC1_CH4"},
    {4,  GPIO_NUM_6,  5, 1, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC1 | PIN_CAP_PWM, "SW4/GPIO6/ADC1_CH5"},
    {5,  GPIO_NUM_7,  6, 1, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC1 | PIN_CAP_PWM, "SW5/GPIO7/ADC1_CH6"},
    {6,  GPIO_NUM_8,  7, 1, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC1 | PIN_CAP_PWM | PIN_CAP_I2C, "SW6/GPIO8/ADC1_CH7/SDA"},
    {7,  GPIO_NUM_9,  8, 1, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC1 | PIN_CAP_PWM | PIN_CAP_I2C, "SW7/GPIO9/ADC1_CH8/SCL"},
    {8,  GPIO_NUM_10, 9, 1, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC1 | PIN_CAP_PWM, "SW8/GPIO10/ADC1_CH9"},
    {9,  GPIO_NUM_11, 0, 2, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC2 | PIN_CAP_PWM, "SW9/GPIO11/ADC2_CH0"},
    {10, GPIO_NUM_12, 1, 2, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC2 | PIN_CAP_PWM, "SW10/GPIO12/ADC2_CH1"},
    {11, GPIO_NUM_13, 2, 2, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC2 | PIN_CAP_PWM, "SW11/GPIO13/ADC2_CH2"},
    {12, GPIO_NUM_14, 3, 2, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC2 | PIN_CAP_PWM, "SW12/GPIO14/ADC2_CH3"},
    {13, GPIO_NUM_15, 4, 2, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC2 | PIN_CAP_PWM, "SW13/GPIO15/ADC2_CH4"},
    {14, GPIO_NUM_16, 5, 2, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC2 | PIN_CAP_PWM, "SW14/GPIO16/ADC2_CH5"},
    {15, GPIO_NUM_17, 6, 2, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC2 | PIN_CAP_PWM, "SW15/GPIO17/ADC2_CH6"},
    {16, GPIO_NUM_18, 7, 2, PIN_CAP_DIGITAL | PIN_CAP_ADC | PIN_CAP_ADC2 | PIN_CAP_PWM, "SW16/GPIO18/ADC2_CH7"},
    {17, GPIO_NUM_21, 0xFF, 0, PIN_CAP_DIGITAL | PIN_CAP_PWM, "SW17/GPIO21/Digital"},
    
    // Extended Pins (18-21) - ESP32-Specific (JTAG pins)
    {18, GPIO_NUM_39, 0xFF, 0, PIN_CAP_DIGITAL | PIN_CAP_PWM | PIN_CAP_JTAG, "SW18/GPIO39/JTAG_TDI"},
    {19, GPIO_NUM_40, 0xFF, 0, PIN_CAP_DIGITAL | PIN_CAP_PWM | PIN_CAP_JTAG, "SW19/GPIO40/JTAG_TDO"},
    {20, GPIO_NUM_41, 0xFF, 0, PIN_CAP_DIGITAL | PIN_CAP_PWM | PIN_CAP_JTAG, "SW20/GPIO41/JTAG_TCK"},
    {21, GPIO_NUM_42, 0xFF, 0, PIN_CAP_DIGITAL | PIN_CAP_PWM | PIN_CAP_JTAG, "SW21/GPIO42/JTAG_TMS"},
};

// Initialization flag
static bool gpio_initialized = false;

esp_err_t ESP32_GPIO_Init(void) {
    if (gpio_initialized) {
        ESP_LOGW(TAG, "GPIO already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing GPIO subsystem");
    ESP_LOGI(TAG, "Total pins: %d (Legacy: %d, Extended: %d)", 
             SW_PIN_COUNT, SW_PIN_COUNT_LEGACY, SW_PIN_COUNT_EXTENDED);
    
    // Log pin mapping for debugging
    ESP_LOGI(TAG, "Pin mapping:");
    for (int i = 0; i < SW_PIN_COUNT; i++) {
        ESP_LOGI(TAG, "  Pin %d -> %s", pin_map[i].sw_pin, pin_map[i].name);
        
        // Warn about JTAG pins
        if (pin_map[i].capabilities & PIN_CAP_JTAG) {
            ESP_LOGW(TAG, "    WARNING: Pin %d conflicts with JTAG debugging", pin_map[i].sw_pin);
        }
        
        // Note ADC2/WiFi conflict
        if (pin_map[i].capabilities & PIN_CAP_ADC2) {
            ESP_LOGI(TAG, "    NOTE: Pin %d ADC function conflicts with WiFi (Phase 2+)", pin_map[i].sw_pin);
        }
    }
    
    gpio_initialized = true;
    ESP_LOGI(TAG, "GPIO initialization complete");
    
    return ESP_OK;
}

gpio_num_t ESP32_GPIO_GetGPIONum(uint8_t sw_pin) {
    if (sw_pin >= SW_PIN_COUNT) {
        ESP_LOGE(TAG, "Invalid SW pin: %d (max: %d)", sw_pin, SW_PIN_COUNT - 1);
        return GPIO_NUM_NC;
    }
    return pin_map[sw_pin].gpio_num;
}

uint16_t ESP32_GPIO_GetCapabilities(uint8_t sw_pin) {
    if (sw_pin >= SW_PIN_COUNT) {
        ESP_LOGE(TAG, "Invalid SW pin: %d", sw_pin);
        return 0;
    }
    return pin_map[sw_pin].capabilities;
}

bool ESP32_GPIO_HasCapability(uint8_t sw_pin, uint16_t capability) {
    if (sw_pin >= SW_PIN_COUNT) {
        return false;
    }
    return (pin_map[sw_pin].capabilities & capability) != 0;
}

esp_err_t ESP32_GPIO_SetMode(uint8_t sw_pin, gpio_mode_t mode) {
    gpio_num_t gpio = ESP32_GPIO_GetGPIONum(sw_pin);
    if (gpio == GPIO_NUM_NC) {
        return ESP_FAIL;
    }
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = mode,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure pin %d (GPIO%d): %s", 
                 sw_pin, gpio, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t ESP32_GPIO_SetHigh(uint8_t sw_pin) {
    gpio_num_t gpio = ESP32_GPIO_GetGPIONum(sw_pin);
    if (gpio == GPIO_NUM_NC) {
        return ESP_FAIL;
    }
    return gpio_set_level(gpio, 1);
}

esp_err_t ESP32_GPIO_SetLow(uint8_t sw_pin) {
    gpio_num_t gpio = ESP32_GPIO_GetGPIONum(sw_pin);
    if (gpio == GPIO_NUM_NC) {
        return ESP_FAIL;
    }
    return gpio_set_level(gpio, 0);
}

esp_err_t ESP32_GPIO_SetLevel(uint8_t sw_pin, uint8_t level) {
    if (level) {
        return ESP32_GPIO_SetHigh(sw_pin);
    } else {
        return ESP32_GPIO_SetLow(sw_pin);
    }
}

int ESP32_GPIO_Read(uint8_t sw_pin) {
    gpio_num_t gpio = ESP32_GPIO_GetGPIONum(sw_pin);
    if (gpio == GPIO_NUM_NC) {
        return -1;
    }
    return gpio_get_level(gpio);
}

esp_err_t ESP32_GPIO_SetPullUp(uint8_t sw_pin, bool enable) {
    gpio_num_t gpio = ESP32_GPIO_GetGPIONum(sw_pin);
    if (gpio == GPIO_NUM_NC) {
        return ESP_FAIL;
    }
    
    if (enable) {
        return gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
    } else {
        return gpio_set_pull_mode(gpio, GPIO_FLOATING);
    }
}

esp_err_t ESP32_GPIO_SetPullDown(uint8_t sw_pin, bool enable) {
    gpio_num_t gpio = ESP32_GPIO_GetGPIONum(sw_pin);
    if (gpio == GPIO_NUM_NC) {
        return ESP_FAIL;
    }
    
    if (enable) {
        return gpio_set_pull_mode(gpio, GPIO_PULLDOWN_ONLY);
    } else {
        return gpio_set_pull_mode(gpio, GPIO_FLOATING);
    }
}

esp_err_t ESP32_GPIO_SetOpenDrain(uint8_t sw_pin, bool enable) {
    gpio_num_t gpio = ESP32_GPIO_GetGPIONum(sw_pin);
    if (gpio == GPIO_NUM_NC) {
        return ESP_FAIL;
    }
    
    // Note: This requires reconfiguring the pin mode
    // Open-drain is a mode type in ESP32
    gpio_mode_t mode = enable ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = mode,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    return gpio_config(&io_conf);
}

esp_err_t ESP32_GPIO_GetADCInfo(uint8_t sw_pin, uint8_t* adc_unit, uint8_t* adc_channel) {
    if (sw_pin >= SW_PIN_COUNT) {
        ESP_LOGE(TAG, "Invalid SW pin: %d", sw_pin);
        return ESP_FAIL;
    }
    
    if (!(pin_map[sw_pin].capabilities & PIN_CAP_ADC)) {
        ESP_LOGD(TAG, "Pin %d does not have ADC capability", sw_pin);
        *adc_unit = 0;
        *adc_channel = 0;
        return ESP_FAIL;
    }
    
    *adc_unit = pin_map[sw_pin].adc_unit;
    *adc_channel = pin_map[sw_pin].adc_channel;
    
    return ESP_OK;
}

const sw_pin_map_t* ESP32_GPIO_GetPinMap(void) {
    return pin_map;
}
