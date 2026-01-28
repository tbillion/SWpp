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
#include "hw_abstraction/esp32_uart.h"
#include "hw_abstraction/esp32_i2c.h"

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

/**
 * @brief Test UART initialization
 */
void test_uart_init(void) {
    ESP_LOGI(TAG, "Testing UART initialization");
    
    // UART should already be initialized from ESP32_UART_Init()
    if (ESP32_UART_IsInitialized(ESP32_UART0)) {
        ESP_LOGI(TAG, "  UART0: Initialized (TX=%d, RX=%d, Baud=%d)",
                 ESP32_UART0_TX_PIN, ESP32_UART0_RX_PIN, ESP32_UART_DEFAULT_BAUD);
    } else {
        ESP_LOGW(TAG, "  UART0: NOT initialized");
    }
    
    if (ESP32_UART_IsInitialized(ESP32_UART1)) {
        ESP_LOGI(TAG, "  UART1: Initialized (TX=%d, RX=%d, Baud=%d)",
                 ESP32_UART1_TX_PIN, ESP32_UART1_RX_PIN, ESP32_UART_DEFAULT_BAUD);
    } else {
        ESP_LOGW(TAG, "  UART1: NOT initialized");
    }
    
    ESP_LOGI(TAG, "UART initialization test complete");
}

/**
 * @brief Test UART echo functionality
 */
void test_uart_echo(void) {
    ESP_LOGI(TAG, "Starting UART echo test on UART0");
    ESP_LOGI(TAG, "Type characters (test will read for 5 seconds)...");
    
    uint8_t buffer[128];
    size_t total_echoed = 0;
    
    // Read and echo for 5 seconds
    uint64_t start_time = esp_timer_get_time();
    while ((esp_timer_get_time() - start_time) < 5000000) {
        size_t available = ESP32_UART_Available(ESP32_UART0);
        if (available > 0) {
            // Read data
            size_t to_read = (available > sizeof(buffer)) ? sizeof(buffer) : available;
            size_t read = ESP32_UART_Read(ESP32_UART0, buffer, to_read);
            
            if (read > 0) {
                // Echo it back
                ESP32_UART_Write(ESP32_UART0, buffer, read);
                total_echoed += read;
            }
        }
        
        // Small delay to prevent tight loop
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "UART echo test complete:");
    ESP_LOGI(TAG, "  Echoed %zu characters", total_echoed);
}

/**
 * @brief Test UART statistics
 */
void test_uart_stats(void) {
    ESP_LOGI(TAG, "Testing UART statistics");
    
    ESP32_UART_Stats_t stats0, stats1;
    
    if (ESP32_UART_GetStats(ESP32_UART0, &stats0) == ESP_OK) {
        ESP_LOGI(TAG, "UART0 statistics:");
        ESP_LOGI(TAG, "  TX bytes: %lu", stats0.tx_bytes);
        ESP_LOGI(TAG, "  RX bytes: %lu", stats0.rx_bytes);
        ESP_LOGI(TAG, "  Overflows: %lu", stats0.rx_overflows);
        ESP_LOGI(TAG, "  Errors: PE=%lu, FE=%lu, BRK=%lu",
                 stats0.parity_errors, stats0.frame_errors, stats0.break_conditions);
    }
    
    if (ESP32_UART_GetStats(ESP32_UART1, &stats1) == ESP_OK) {
        ESP_LOGI(TAG, "UART1 statistics:");
        ESP_LOGI(TAG, "  TX bytes: %lu", stats1.tx_bytes);
        ESP_LOGI(TAG, "  RX bytes: %lu", stats1.rx_bytes);
        ESP_LOGI(TAG, "  Overflows: %lu", stats1.rx_overflows);
        ESP_LOGI(TAG, "  Errors: PE=%lu, FE=%lu, BRK=%lu",
                 stats1.parity_errors, stats1.frame_errors, stats1.break_conditions);
    }
    
    ESP_LOGI(TAG, "UART statistics test complete");
}

/**
 * @brief I2C packet callback (for testing)
 * 
 * Simple echo callback that returns the received packet
 */
static bool test_i2c_callback(const uint8_t* rx_packet, uint8_t* tx_packet) {
    // Echo the packet back (for testing)
    memcpy(tx_packet, rx_packet, ESP32_I2C_PACKET_SIZE);
    
    // Log the received packet
    ESP_LOGI(TAG, "I2C RX: %02X %02X %02X %02X %02X %02X %02X %02X",
             rx_packet[0], rx_packet[1], rx_packet[2], rx_packet[3],
             rx_packet[4], rx_packet[5], rx_packet[6], rx_packet[7]);
    
    return true; // Response ready
}

/**
 * @brief Test I2C initialization and address selection
 */
void test_i2c_init(void) {
    ESP_LOGI(TAG, "Testing I2C initialization");
    
    if (ESP32_I2C_IsInitialized()) {
        uint8_t addr = ESP32_I2C_GetAddress();
        uint8_t addr_offset = ESP32_I2C_ReadAddressPins();
        ESP32_I2C_Tier_t tier = ESP32_I2C_GetTier();
        
        ESP_LOGI(TAG, "  I2C: Initialized (SDA=%d, SCL=%d)",
                 ESP32_I2C_SDA_PIN, ESP32_I2C_SCL_PIN);
        ESP_LOGI(TAG, "  Address pins: GPIO %d-%d",
                 ESP32_I2C_ADDR_A0_PIN, ESP32_I2C_ADDR_A3_PIN);
        ESP_LOGI(TAG, "  Address offset: 0x%X", addr_offset);
        ESP_LOGI(TAG, "  Slave address: 0x%02X", addr);
        ESP_LOGI(TAG, "  Implementation: Tier %d (1=HW, 2=Opt, 3=SW)", tier);
    } else {
        ESP_LOGW(TAG, "  I2C: NOT initialized");
    }
    
    ESP_LOGI(TAG, "I2C initialization test complete");
}

/**
 * @brief Test I2C slave mode (waits for host requests)
 */
void test_i2c_slave(void) {
    ESP_LOGI(TAG, "Starting I2C slave test");
    ESP_LOGI(TAG, "Listening for I2C master requests (10 seconds)...");
    ESP_LOGI(TAG, "Connect I2C master and send 8-byte packets");
    
    // Set callback for packet processing
    ESP32_I2C_SetPacketCallback(test_i2c_callback);
    
    // Monitor for 10 seconds
    uint64_t start_time = esp_timer_get_time();
    uint32_t last_rx_count = 0;
    
    while ((esp_timer_get_time() - start_time) < 10000000) {
        // Check for packets
        if (ESP32_I2C_HasPacket()) {
            uint8_t packet[ESP32_I2C_PACKET_SIZE];
            if (ESP32_I2C_ReadPacket(packet)) {
                ESP_LOGI(TAG, "Packet received via buffer check");
            }
        }
        
        // Check stats periodically
        ESP32_I2C_Stats_t stats;
        ESP32_I2C_GetStats(&stats);
        if (stats.packets_received != last_rx_count) {
            ESP_LOGI(TAG, "Packets RX: %lu, TX: %lu",
                     stats.packets_received, stats.packets_transmitted);
            last_rx_count = stats.packets_received;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "I2C slave test complete");
}

/**
 * @brief Test I2C statistics
 */
void test_i2c_stats(void) {
    ESP_LOGI(TAG, "Testing I2C statistics");
    
    ESP32_I2C_Stats_t stats;
    ESP32_I2C_GetStats(&stats);
    
    ESP_LOGI(TAG, "I2C statistics:");
    ESP_LOGI(TAG, "  Packets RX: %lu", stats.packets_received);
    ESP_LOGI(TAG, "  Packets TX: %lu", stats.packets_transmitted);
    ESP_LOGI(TAG, "  Incomplete: %lu", stats.incomplete_packets);
    ESP_LOGI(TAG, "  Overflows: %lu", stats.buffer_overflows);
    ESP_LOGI(TAG, "  ACK errors: %lu", stats.ack_errors);
    ESP_LOGI(TAG, "  Bus errors: %lu", stats.bus_errors);
    ESP_LOGI(TAG, "  Timeouts: %lu", stats.timeouts);
    ESP_LOGI(TAG, "  Tier switches: %lu", stats.tier_switches);
    ESP_LOGI(TAG, "  Current tier: %d (1=HW, 2=Opt, 3=SW)", stats.current_tier);
    
    ESP_LOGI(TAG, "I2C statistics test complete");
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
    
    // Initialize UART subsystem
    ESP_LOGI(TAG, "Initializing UART subsystem...");
    ret = ESP32_UART_Init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART initialization failed!");
        return;
    }
    
    // Initialize I2C subsystem
    ESP_LOGI(TAG, "Initializing I2C subsystem...");
    ret = ESP32_I2C_Init() ? ESP_OK : ESP_FAIL;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed!");
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
    
    // Run UART tests
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== UART TESTS ===");
    
    test_uart_init();
    ESP_LOGI(TAG, "");
    
    test_uart_echo();
    ESP_LOGI(TAG, "");
    
    test_uart_stats();
    ESP_LOGI(TAG, "");
    
    // Run I2C tests
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== I2C TESTS ===");
    
    test_i2c_init();
    ESP_LOGI(TAG, "");
    
    test_i2c_slave();
    ESP_LOGI(TAG, "");
    
    test_i2c_stats();
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "All HAL tests complete!");
    ESP_LOGI(TAG, "Phase 2 progress: GPIO + Timers + UART + I2C (56%%)");
    ESP_LOGI(TAG, "Next: ADC abstraction (esp32_adc.c/h)");
    ESP_LOGI(TAG, "Note: Build with ESP-IDF 5.x to test on hardware");
    ESP_LOGI(TAG, "  $ idf.py build flash monitor");
    
    // Main loop (for now, just idle)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
