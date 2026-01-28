/**
 * @file esp32_timers.c
 * @brief ESP32-S3 Timer Hardware Abstraction Layer Implementation
 * 
 * Copyright 2020-2026 Broadwell Consulting Inc.
 * 
 * Implements dual timer system per TIMING_MODEL.md:
 * - 1ms foreground timer: Triggers ProcessPins() via semaphore
 * - 57.6kHz DMA timer: High-speed GPIO sampling
 * 
 * Both ISRs are IRAM_ATTR for zero cache miss and minimal jitter.
 */

#include "esp32_timers.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include <string.h>

static const char* TAG = "SW_TIMERS";

// Timer state
static bool timer_1ms_running = false;
static bool timer_dma_running = false;

// Callbacks
static timer_1ms_callback_t callback_1ms = NULL;
static timer_dma_callback_t callback_dma = NULL;

// Semaphore for 1ms timer (alternative to callback)
static SemaphoreHandle_t semaphore_1ms = NULL;

// Statistics (volatile for ISR access)
static volatile uint64_t cycles_1ms_count = 0;
static volatile uint64_t cycles_dma_count = 0;
static volatile uint32_t overflow_count = 0;
static volatile uint32_t max_exec_time = 0;

/**
 * @brief 1ms timer ISR - IRAM for zero cache miss
 * 
 * Signals foreground task via semaphore or callback.
 * Expected execution time: <10µs
 */
static void IRAM_ATTR timer_1ms_isr(void* arg) {
    // Clear interrupt (required before any other operations)
    timer_group_clr_intr_status_in_isr(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX);
    
    // Re-enable alarm for next cycle
    timer_group_enable_alarm_in_isr(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX);
    
    // Increment cycle counter
    cycles_1ms_count++;
    
    // Signal via semaphore (preferred method)
    if (semaphore_1ms != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(semaphore_1ms, &xHigherPriorityTaskWoken);
        
        // Yield if a higher priority task was woken
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
    
    // Call callback if registered (alternative method)
    if (callback_1ms != NULL) {
        callback_1ms();
    }
}

/**
 * @brief DMA timer ISR - IRAM for zero cache miss
 * 
 * Performs high-speed GPIO sampling at 57.6kHz.
 * CRITICAL: Must complete in <5µs (17.36µs period budget)
 */
static void IRAM_ATTR timer_dma_isr(void* arg) {
    // Clear interrupt (required)
    timer_group_clr_intr_status_in_isr(DMA_TIMER_GROUP, DMA_TIMER_INDEX);
    
    // Re-enable alarm for next cycle
    timer_group_enable_alarm_in_isr(DMA_TIMER_GROUP, DMA_TIMER_INDEX);
    
    // Increment cycle counter
    cycles_dma_count++;
    
    // Get current timestamp
    uint64_t timestamp = esp_timer_get_time();
    
    // Call callback if registered
    if (callback_dma != NULL) {
        callback_dma(timestamp);
    }
}

esp_err_t ESP32_Timers_Init(void) {
    ESP_LOGI(TAG, "Initializing timer subsystem");
    
    // Configure 1ms foreground timer
    timer_config_t config_1ms = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    };
    
    esp_err_t ret = timer_init(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX, &config_1ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init 1ms timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set counter value to 0
    timer_set_counter_value(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX, 0);
    
    // Set alarm value (1000µs = 1ms)
    timer_set_alarm_value(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX, TIMER_1MS_PERIOD_US);
    
    // Enable timer interrupt
    timer_enable_intr(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX);
    
    // Register ISR
    ret = timer_isr_register(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX,
                            timer_1ms_isr, NULL, ESP_INTR_FLAG_IRAM, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register 1ms ISR: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "1ms timer configured (TIMG%d_T%d, period=%dus)",
             FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX, TIMER_1MS_PERIOD_US);
    
    // Configure 57.6kHz DMA timer
    timer_config_t config_dma = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    };
    
    ret = timer_init(DMA_TIMER_GROUP, DMA_TIMER_INDEX, &config_dma);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init DMA timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set counter value to 0
    timer_set_counter_value(DMA_TIMER_GROUP, DMA_TIMER_INDEX, 0);
    
    // Set alarm value (17µs ≈ 57.6kHz)
    timer_set_alarm_value(DMA_TIMER_GROUP, DMA_TIMER_INDEX, TIMER_DMA_PERIOD_US);
    
    // Enable timer interrupt
    timer_enable_intr(DMA_TIMER_GROUP, DMA_TIMER_INDEX);
    
    // Register ISR
    ret = timer_isr_register(DMA_TIMER_GROUP, DMA_TIMER_INDEX,
                            timer_dma_isr, NULL, ESP_INTR_FLAG_IRAM, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register DMA ISR: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "DMA timer configured (TIMG%d_T%d, period=%dus ≈ %.1fkHz)",
             DMA_TIMER_GROUP, DMA_TIMER_INDEX, TIMER_DMA_PERIOD_US,
             1000000.0 / TIMER_DMA_PERIOD_US);
    
    ESP_LOGI(TAG, "Timer subsystem initialized");
    
    return ESP_OK;
}

esp_err_t ESP32_Timers_Start1ms(timer_1ms_callback_t callback, SemaphoreHandle_t semaphore) {
    if (timer_1ms_running) {
        ESP_LOGW(TAG, "1ms timer already running");
        return ESP_OK;
    }
    
    if (callback == NULL && semaphore == NULL) {
        ESP_LOGE(TAG, "Must provide either callback or semaphore");
        return ESP_FAIL;
    }
    
    // Store callback and semaphore
    callback_1ms = callback;
    semaphore_1ms = semaphore;
    
    // Reset counter
    timer_set_counter_value(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX, 0);
    cycles_1ms_count = 0;
    
    // Start timer
    esp_err_t ret = timer_start(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start 1ms timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    timer_1ms_running = true;
    ESP_LOGI(TAG, "1ms timer started");
    
    return ESP_OK;
}

esp_err_t ESP32_Timers_Stop1ms(void) {
    if (!timer_1ms_running) {
        ESP_LOGW(TAG, "1ms timer not running");
        return ESP_OK;
    }
    
    esp_err_t ret = timer_pause(FOREGROUND_TIMER_GROUP, FOREGROUND_TIMER_INDEX);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop 1ms timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    timer_1ms_running = false;
    callback_1ms = NULL;
    semaphore_1ms = NULL;
    
    ESP_LOGI(TAG, "1ms timer stopped");
    
    return ESP_OK;
}

esp_err_t ESP32_Timers_StartDMA(timer_dma_callback_t callback) {
    if (timer_dma_running) {
        ESP_LOGW(TAG, "DMA timer already running");
        return ESP_OK;
    }
    
    if (callback == NULL) {
        ESP_LOGE(TAG, "DMA timer requires callback");
        return ESP_FAIL;
    }
    
    // Store callback
    callback_dma = callback;
    
    // Reset counter
    timer_set_counter_value(DMA_TIMER_GROUP, DMA_TIMER_INDEX, 0);
    cycles_dma_count = 0;
    
    // Start timer
    esp_err_t ret = timer_start(DMA_TIMER_GROUP, DMA_TIMER_INDEX);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start DMA timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    timer_dma_running = true;
    ESP_LOGI(TAG, "DMA timer started (%.1fkHz)", 1000000.0 / TIMER_DMA_PERIOD_US);
    ESP_LOGW(TAG, "DMA timer will consume ~23%% of one CPU core");
    
    return ESP_OK;
}

esp_err_t ESP32_Timers_StopDMA(void) {
    if (!timer_dma_running) {
        ESP_LOGW(TAG, "DMA timer not running");
        return ESP_OK;
    }
    
    esp_err_t ret = timer_pause(DMA_TIMER_GROUP, DMA_TIMER_INDEX);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop DMA timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    timer_dma_running = false;
    callback_dma = NULL;
    
    ESP_LOGI(TAG, "DMA timer stopped");
    
    return ESP_OK;
}

uint64_t ESP32_Timers_GetTimestampUs(void) {
    return esp_timer_get_time();
}

esp_err_t ESP32_Timers_GetStats(timer_stats_t* stats) {
    if (stats == NULL) {
        return ESP_FAIL;
    }
    
    stats->cycles_1ms = cycles_1ms_count;
    stats->cycles_dma = cycles_dma_count;
    stats->overflows_detected = overflow_count;
    stats->max_exec_time_us = max_exec_time;
    
    return ESP_OK;
}

void ESP32_Timers_ResetStats(void) {
    cycles_1ms_count = 0;
    cycles_dma_count = 0;
    overflow_count = 0;
    max_exec_time = 0;
}

uint64_t ESP32_Timers_Get1msCycles(void) {
    return cycles_1ms_count;
}

uint64_t ESP32_Timers_GetDMACycles(void) {
    return cycles_dma_count;
}

bool ESP32_Timers_Is1msRunning(void) {
    return timer_1ms_running;
}

bool ESP32_Timers_IsDMARunning(void) {
    return timer_dma_running;
}
