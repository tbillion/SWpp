/**
 * @file main.c
 * @brief Serial Wombat ESP32-S3 Main Entry Point
 * 
 * Copyright 2020-2026 Broadwell Consulting Inc.
 * 
 * Phase 2 Complete: Hardware Abstraction Layer with System Integration
 * This demonstrates the complete HAL with FreeRTOS task architecture.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "hw_abstraction/esp32_system.h"
#include "hw_abstraction/esp32_gpio.h"
#include "hw_abstraction/esp32_timers.h"
#include "hw_abstraction/esp32_uart.h"
#include "hw_abstraction/esp32_i2c.h"
#include "hw_abstraction/esp32_adc.h"
#include "hw_abstraction/esp32_dma.h"

static const char* TAG = "SW_MAIN";

// ============================================================================
// HAL COMPONENT TESTS (from previous sessions)
// ============================================================================

/**
 * @brief Test GPIO operations
 */
void test_gpio(void) {
    ESP_LOGI(TAG, "=== GPIO Test ===");
    ESP_LOGI(TAG, "SW Pin 0 (GPIO1) configured as output");
    ESP_LOGI(TAG, "Blinking 5 times...");
    
    ESP32_GPIO_SetMode(0, ESP32_GPIO_MODE_OUTPUT);
    
    for (int i = 0; i < 5; i++) {
        ESP32_GPIO_Write(0, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        ESP32_GPIO_Write(0, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    ESP_LOGI(TAG, "GPIO test complete");
}

/**
 * @brief Test System health monitoring
 */
void test_system_health(void) {
    ESP_LOGI(TAG, "=== System Health Test ===");
    
    ESP32_System_Health_t health;
    ESP32_System_GetHealth(&health);
    
    ESP_LOGI(TAG, "Foreground cycles: %lu", health.foreground_cycles);
    ESP_LOGI(TAG, "Missed cycles: %lu", health.foreground_missed);
    ESP_LOGI(TAG, "Overruns: %lu", health.foreground_overruns);
    ESP_LOGI(TAG, "RX packets: %lu", health.rx_packets_processed);
    ESP_LOGI(TAG, "Uptime: %lu ms", health.system_uptime_ms);
    ESP_LOGI(TAG, "Free heap: %lu bytes", health.free_heap_bytes);
    ESP_LOGI(TAG, "Supervisor: %s", health.supervisor_enabled ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "System healthy: %s", health.system_healthy ? "Yes" : "No");
}

/**
 * @brief Test task statistics
 */
void test_system_tasks(void) {
    ESP_LOGI(TAG, "=== Task Statistics Test ===");
    
    ESP32_System_TaskStats_t stats[5];
    uint32_t count = ESP32_System_GetTaskStats(stats);
    
    ESP_LOGI(TAG, "Active tasks: %lu", count);
    for (uint32_t i = 0; i < count; i++) {
        const char* state_str = "Unknown";
        switch (stats[i].state) {
            case eRunning: state_str = "Running"; break;
            case eReady: state_str = "Ready"; break;
            case eBlocked: state_str = "Blocked"; break;
            case eSuspended: state_str = "Suspended"; break;
            case eDeleted: state_str = "Deleted"; break;
        }
        
        ESP_LOGI(TAG, "Task %lu: %s", i, stats[i].name);
        ESP_LOGI(TAG, "  State: %s", state_str);
        ESP_LOGI(TAG, "  Priority: %lu", stats[i].priority);
        ESP_LOGI(TAG, "  Stack free: %lu bytes", stats[i].stack_high_water_mark);
        ESP_LOGI(TAG, "  Core: %d", stats[i].core_id);
    }
}

/**
 * @brief Quick UART test
 */
void test_uart_quick(void) {
    ESP_LOGI(TAG, "=== UART Quick Test ===");
    ESP_LOGI(TAG, "UART0 initialized: %s", ESP32_UART_IsInitialized(0) ? "Yes" : "No");
    ESP_LOGI(TAG, "UART1 initialized: %s", ESP32_UART_IsInitialized(1) ? "Yes" : "No");
    
    // Send test message
    ESP32_UART_WriteString(0, "Serial Wombat ESP32-S3 - UART Test\r\n");
}

/**
 * @brief Quick I2C test
 */
void test_i2c_quick(void) {
    ESP_LOGI(TAG, "=== I2C Quick Test ===");
    uint8_t addr = ESP32_I2C_GetAddress();
    ESP_LOGI(TAG, "I2C slave address: 0x%02X", addr);
    ESP_LOGI(TAG, "I2C initialized: %s", ESP32_I2C_IsInitialized() ? "Yes" : "No");
}

/**
 * @brief Quick ADC test
 */
void test_adc_quick(void) {
    ESP_LOGI(TAG, "=== ADC Quick Test ===");
    ESP_LOGI(TAG, "Reading first 3 ADC channels:");
    
    for (int pin = 0; pin < 3; pin++) {
        if (ESP32_ADC_IsAvailable(pin)) {
            uint16_t raw = ESP32_ADC_ReadRaw(pin);
            uint16_t scaled = ESP32_ADC_Read16Bit(pin);
            int mv = ESP32_ADC_ReadMillivolts(pin);
            ESP_LOGI(TAG, "Pin %d: Raw=%u, Scaled=%u, Voltage=%dmV", pin, raw, scaled, mv);
        }
    }
}

/**
 * @brief Quick DMA test
 */
void test_dma_quick(void) {
    ESP_LOGI(TAG, "=== DMA Quick Test ===");
    ESP_LOGI(TAG, "DMA initialized: %s", ESP32_DMA_IsInitialized() ? "Yes" : "No");
    ESP_LOGI(TAG, "DMA running: %s", ESP32_DMA_IsRunning() ? "Yes" : "No");
    ESP_LOGI(TAG, "Buffer capacity: %lu samples", ESP32_DMA_GetCapacity());
    ESP_LOGI(TAG, "Samples available: %lu", ESP32_DMA_Available());
    
    ESP32_DMA_Stats_t stats;
    ESP32_DMA_GetStats(&stats);
    ESP_LOGI(TAG, "Total captured: %lu samples", stats.samples_captured);
    ESP_LOGI(TAG, "Buffer overflows: %lu", stats.buffer_overflows);
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

/**
 * @brief Main application entry point
 */
void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Serial Wombat ESP32-S3 Port");
    ESP_LOGI(TAG, "Phase 2: Hardware Abstraction Layer");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize system (all HAL components)
    ESP_LOGI(TAG, "Initializing system...");
    if (!ESP32_System_Init()) {
        ESP_LOGE(TAG, "System initialization failed!");
        return;
    }
    
    // Start FreeRTOS tasks
    ESP_LOGI(TAG, "Starting tasks...");
    if (!ESP32_System_StartTasks()) {
        ESP_LOGE(TAG, "Failed to start tasks!");
        return;
    }
    
    ESP_LOGI(TAG, "System running!");
    ESP_LOGI(TAG, "");
    
    // Wait a bit for tasks to stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Run quick tests
    ESP_LOGI(TAG, "Running quick component tests...");
    ESP_LOGI(TAG, "");
    
    test_system_health();
    ESP_LOGI(TAG, "");
    
    test_system_tasks();
    ESP_LOGI(TAG, "");
    
    test_gpio();
    ESP_LOGI(TAG, "");
    
    test_uart_quick();
    ESP_LOGI(TAG, "");
    
    test_i2c_quick();
    ESP_LOGI(TAG, "");
    
    test_adc_quick();
    ESP_LOGI(TAG, "");
    
    test_dma_quick();
    ESP_LOGI(TAG, "");
    
    // Main monitoring loop
    ESP_LOGI(TAG, "Entering monitoring loop (updates every 5 seconds)...");
    ESP_LOGI(TAG, "");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        ESP_LOGI(TAG, "=== System Status (every 5s) ===");
        
        ESP32_System_Health_t health;
        ESP32_System_GetHealth(&health);
        
        ESP_LOGI(TAG, "Uptime: %lu ms (%.1f sec)", 
                 health.system_uptime_ms,
                 health.system_uptime_ms / 1000.0);
        ESP_LOGI(TAG, "Foreground: %lu cycles (%lu Hz)", 
                 health.foreground_cycles,
                 health.foreground_cycles * 1000 / health.system_uptime_ms);
        ESP_LOGI(TAG, "Missed: %lu, Overruns: %lu", 
                 health.foreground_missed, 
                 health.foreground_overruns);
        ESP_LOGI(TAG, "Free heap: %lu KB", health.free_heap_bytes / 1024);
        ESP_LOGI(TAG, "Health: %s", health.system_healthy ? "OK" : "WARNING");
        
        ESP32_DMA_Stats_t dma_stats;
        ESP32_DMA_GetStats(&dma_stats);
        ESP_LOGI(TAG, "DMA: %lu samples captured, %lu available", 
                 dma_stats.samples_captured,
                 ESP32_DMA_Available());
        
        ESP_LOGI(TAG, "");
    }
}
