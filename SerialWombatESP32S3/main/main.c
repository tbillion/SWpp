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
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "hw_abstraction/esp32_gpio.h"
#include "hw_abstraction/esp32_timers.h"

static const char* TAG = "SW_MAIN";

// Semaphore for 1ms timer
static SemaphoreHandle_t timer_semaphore = NULL;

// Test counters
static volatile uint32_t timer_1ms_count = 0;
static volatile uint32_t timer_dma_count = 0;

/**
 * @brief Callback for DMA timer test
 */
static void IRAM_ATTR dma_timer_callback(uint64_t timestamp_us) {
    timer_dma_count++;
    // In real use, this would sample GPIO and write to circular buffer
}

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

/**
 * @brief Test 1ms timer with semaphore
 */
void test_timer_1ms(void) {
    ESP_LOGI(TAG, "Starting 1ms timer test");
    
    // Create semaphore
    timer_semaphore = xSemaphoreCreateBinary();
    if (timer_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }
    
    // Start 1ms timer
    esp_err_t ret = ESP32_Timers_Start1ms(NULL, timer_semaphore);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start 1ms timer");
        vSemaphoreDelete(timer_semaphore);
        return;
    }
    
    // Wait for 10 cycles and count them
    uint64_t start_time = esp_timer_get_time();
    for (int i = 0; i < 10; i++) {
        if (xSemaphoreTake(timer_semaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
            timer_1ms_count++;
            ESP_LOGI(TAG, "1ms cycle %lu (total: %llu)",
                     timer_1ms_count, ESP32_Timers_Get1msCycles());
        } else {
            ESP_LOGE(TAG, "Timeout waiting for 1ms timer");
            break;
        }
    }
    uint64_t end_time = esp_timer_get_time();
    
    // Stop timer
    ESP32_Timers_Stop1ms();
    
    // Calculate actual period
    uint64_t elapsed_us = end_time - start_time;
    float avg_period_ms = elapsed_us / (float)(timer_1ms_count * 1000);
    
    ESP_LOGI(TAG, "1ms timer test complete:");
    ESP_LOGI(TAG, "  Cycles: %lu", timer_1ms_count);
    ESP_LOGI(TAG, "  Elapsed: %llu us", elapsed_us);
    ESP_LOGI(TAG, "  Avg period: %.3f ms", avg_period_ms);
    ESP_LOGI(TAG, "  Deviation: %.1f us", (avg_period_ms - 1.0) * 1000);
    
    // Clean up
    vSemaphoreDelete(timer_semaphore);
    timer_semaphore = NULL;
}

/**
 * @brief Test 57.6kHz DMA timer
 */
void test_timer_dma(void) {
    ESP_LOGI(TAG, "Starting DMA timer test (57.6kHz)");
    
    // Reset counter
    timer_dma_count = 0;
    
    // Start DMA timer
    esp_err_t ret = ESP32_Timers_StartDMA(dma_timer_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start DMA timer");
        return;
    }
    
    // Let it run for 1 second
    uint64_t start_time = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(1000));
    uint64_t end_time = esp_timer_get_time();
    
    // Stop timer
    ESP32_Timers_StopDMA();
    
    // Calculate frequency
    uint64_t elapsed_us = end_time - start_time;
    float actual_freq_khz = (timer_dma_count * 1000000.0) / elapsed_us / 1000.0;
    float target_freq_khz = 1000000.0 / TIMER_DMA_PERIOD_US / 1000.0;
    
    ESP_LOGI(TAG, "DMA timer test complete:");
    ESP_LOGI(TAG, "  Cycles: %lu", timer_dma_count);
    ESP_LOGI(TAG, "  Elapsed: %llu us (%.3f s)", elapsed_us, elapsed_us / 1000000.0);
    ESP_LOGI(TAG, "  Target freq: %.1f kHz", target_freq_khz);
    ESP_LOGI(TAG, "  Actual freq: %.1f kHz", actual_freq_khz);
    ESP_LOGI(TAG, "  Error: %.1f%%", (actual_freq_khz - target_freq_khz) / target_freq_khz * 100);
}

/**
 * @brief Test timer statistics
 */
void test_timer_stats(void) {
    ESP_LOGI(TAG, "Testing timer statistics");
    
    timer_stats_t stats;
    esp_err_t ret = ESP32_Timers_GetStats(&stats);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get timer stats");
        return;
    }
    
    ESP_LOGI(TAG, "Timer statistics:");
    ESP_LOGI(TAG, "  1ms cycles: %llu", stats.cycles_1ms);
    ESP_LOGI(TAG, "  DMA cycles: %llu", stats.cycles_dma);
    ESP_LOGI(TAG, "  Overflows: %lu", stats.overflows_detected);
    ESP_LOGI(TAG, "  Max exec time: %lu us", stats.max_exec_time_us);
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
    
    // Initialize Timer subsystem
    ESP_LOGI(TAG, "Initializing Timer subsystem...");
    ret = ESP32_Timers_Init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Timer initialization failed!");
        return;
    }
    
    // Run GPIO tests
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== GPIO TESTS ===");
    
    test_gpio_capabilities();
    ESP_LOGI(TAG, "");
    
    test_gpio_blink();
    ESP_LOGI(TAG, "");
    
    test_gpio_read();
    ESP_LOGI(TAG, "");
    
    // Run Timer tests
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== TIMER TESTS ===");
    
    test_timer_1ms();
    ESP_LOGI(TAG, "");
    
    test_timer_dma();
    ESP_LOGI(TAG, "");
    
    test_timer_stats();
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "All HAL tests complete!");
    ESP_LOGI(TAG, "Phase 2 progress: GPIO + Timers complete");
    ESP_LOGI(TAG, "Next: UART abstraction (esp32_uart.c/h)");
    
    // Main loop (for now, just idle)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
