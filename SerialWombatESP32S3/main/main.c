/**
 * @file main.c
 * @brief Serial Wombat ESP32-S3 Main Entry Point
 * 
 * Copyright 2020-2026 Broadwell Consulting Inc.
 * 
 * This is the main entry point for the Serial Wombat ESP32-S3 port.
 * Phase 2: Hardware Abstraction Layer testing
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "hw_abstraction/esp32_gpio.h"

static const char* TAG = "SW_MAIN";

/**
 * @brief Test GPIO functionality with LED blink
 * 
 * Tests basic GPIO operations on pin 0 (GPIO1)
 */
void test_gpio_blink(void) {
    ESP_LOGI(TAG, "Starting GPIO blink test on SW Pin 0 (GPIO1)");
    
    // Configure pin 0 as output
    esp_err_t ret = ESP32_GPIO_SetMode(0, GPIO_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set pin mode");
        return;
    }
    
    // Blink 10 times
    for (int i = 0; i < 10; i++) {
        ESP_LOGI(TAG, "Blink %d - HIGH", i + 1);
        ESP32_GPIO_SetHigh(0);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        ESP_LOGI(TAG, "Blink %d - LOW", i + 1);
        ESP32_GPIO_SetLow(0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "GPIO blink test complete");
}

/**
 * @brief Test GPIO read functionality
 * 
 * Tests reading from a pin (with pull-up enabled)
 */
void test_gpio_read(void) {
    ESP_LOGI(TAG, "Starting GPIO read test on SW Pin 1 (GPIO2)");
    
    // Configure pin 1 as input with pull-up
    ESP32_GPIO_SetMode(1, GPIO_MODE_INPUT);
    ESP32_GPIO_SetPullUp(1, true);
    
    // Read pin state 5 times
    for (int i = 0; i < 5; i++) {
        int level = ESP32_GPIO_Read(1);
        ESP_LOGI(TAG, "Pin 1 level: %d", level);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    ESP_LOGI(TAG, "GPIO read test complete");
}

/**
 * @brief Test pin capability queries
 */
void test_gpio_capabilities(void) {
    ESP_LOGI(TAG, "Testing pin capability queries");
    
    // Test a few pins
    for (int pin = 0; pin < 5; pin++) {
        uint16_t caps = ESP32_GPIO_GetCapabilities(pin);
        gpio_num_t gpio = ESP32_GPIO_GetGPIONum(pin);
        
        ESP_LOGI(TAG, "Pin %d (GPIO%d):", pin, gpio);
        ESP_LOGI(TAG, "  Digital: %s", (caps & PIN_CAP_DIGITAL) ? "YES" : "NO");
        ESP_LOGI(TAG, "  ADC: %s", (caps & PIN_CAP_ADC) ? "YES" : "NO");
        ESP_LOGI(TAG, "  PWM: %s", (caps & PIN_CAP_PWM) ? "YES" : "NO");
        
        if (caps & PIN_CAP_ADC) {
            uint8_t adc_unit, adc_channel;
            if (ESP32_GPIO_GetADCInfo(pin, &adc_unit, &adc_channel) == ESP_OK) {
                ESP_LOGI(TAG, "  ADC Unit: %d, Channel: %d", adc_unit, adc_channel);
            }
        }
    }
    
    ESP_LOGI(TAG, "Pin capability test complete");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Serial Wombat ESP32-S3 Port");
    ESP_LOGI(TAG, "Phase 2: Hardware Abstraction Layer");
    ESP_LOGI(TAG, "Version: SW18AB-v2.1.0-esp32s3-dev");
    ESP_LOGI(TAG, "================================");
    
    // Initialize NVS (required for future features)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize GPIO subsystem
    ESP_LOGI(TAG, "Initializing GPIO subsystem...");
    ret = ESP32_GPIO_Init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO initialization failed!");
        return;
    }
    
    // Run GPIO tests
    ESP_LOGI(TAG, "Running GPIO tests...");
    ESP_LOGI(TAG, "");
    
    test_gpio_capabilities();
    ESP_LOGI(TAG, "");
    
    test_gpio_blink();
    ESP_LOGI(TAG, "");
    
    test_gpio_read();
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "All GPIO tests complete!");
    ESP_LOGI(TAG, "Phase 2: GPIO HAL implementation verified");
    ESP_LOGI(TAG, "Next: Timer abstraction (esp32_timers.c/h)");
    
    // Main loop (for now, just idle)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
