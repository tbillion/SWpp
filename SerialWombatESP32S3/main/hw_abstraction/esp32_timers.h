/**
 * @file esp32_timers.h
 * @brief ESP32-S3 Timer Hardware Abstraction Layer for Serial Wombat
 * 
 * Copyright 2020-2026 Broadwell Consulting Inc.
 * 
 * This file provides timer abstraction for Serial Wombat timing requirements:
 * - 1ms foreground timer for ProcessPins() execution
 * - 57.6kHz DMA timer for high-speed GPIO sampling
 * 
 * Architecture (per TIMING_MODEL.md):
 * - Timer Group 0, Timer 0: 1ms periodic (foreground trigger)
 * - Timer Group 1, Timer 0: 17.36µs periodic (57.6kHz DMA sampling)
 * - Both use IRAM_ATTR ISRs for zero cache miss
 * 
 * Governed by: PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md Section 6
 */

#ifndef ESP32_TIMERS_H
#define ESP32_TIMERS_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/timer.h"

#ifdef __cplusplus
extern "C" {
#endif

// Timer configuration constants
#define TIMER_DIVIDER         80                // 80MHz / 80 = 1MHz (1µs tick)
#define TIMER_1MS_PERIOD_US   1000              // 1000µs = 1ms
#define TIMER_DMA_PERIOD_US   17                // 17.36µs ≈ 57.6kHz (using 17 for simplicity)

// Timer identifiers
#define FOREGROUND_TIMER_GROUP  TIMER_GROUP_0
#define FOREGROUND_TIMER_INDEX  TIMER_0
#define DMA_TIMER_GROUP         TIMER_GROUP_1
#define DMA_TIMER_INDEX         TIMER_0

/**
 * @brief Callback function type for 1ms foreground timer
 * Called from ISR context, should be kept minimal
 */
typedef void (*timer_1ms_callback_t)(void);

/**
 * @brief Callback function type for 57.6kHz DMA timer
 * Called from ISR context, must complete in <5µs
 * @param timestamp_us Current timestamp in microseconds
 */
typedef void (*timer_dma_callback_t)(uint64_t timestamp_us);

/**
 * @brief Timer statistics structure
 */
typedef struct {
    uint64_t cycles_1ms;           // Total 1ms timer cycles
    uint64_t cycles_dma;           // Total DMA timer cycles
    uint32_t overflows_detected;   // Detected timing overflows
    uint32_t max_exec_time_us;     // Maximum execution time observed
} timer_stats_t;

/**
 * @brief Initialize timer subsystem
 * Sets up both 1ms and 57.6kHz timers but does not start them
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_Timers_Init(void);

/**
 * @brief Start the 1ms foreground timer
 * 
 * @param callback Optional callback function (can be NULL if using semaphore)
 * @param semaphore Optional semaphore to signal (can be NULL if using callback)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_Timers_Start1ms(timer_1ms_callback_t callback, SemaphoreHandle_t semaphore);

/**
 * @brief Stop the 1ms foreground timer
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_Timers_Stop1ms(void);

/**
 * @brief Start the 57.6kHz DMA timer
 * 
 * @param callback Callback function for DMA sampling (required)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_Timers_StartDMA(timer_dma_callback_t callback);

/**
 * @brief Stop the 57.6kHz DMA timer
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_Timers_StopDMA(void);

/**
 * @brief Get current timestamp in microseconds
 * Uses hardware timer counter for precise timing
 * 
 * @return Current timestamp in microseconds
 */
uint64_t ESP32_Timers_GetTimestampUs(void);

/**
 * @brief Get timer statistics
 * 
 * @param[out] stats Pointer to statistics structure to fill
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ESP32_Timers_GetStats(timer_stats_t* stats);

/**
 * @brief Reset timer statistics
 */
void ESP32_Timers_ResetStats(void);

/**
 * @brief Get 1ms timer cycle count
 * Used for overflow detection
 * 
 * @return Number of 1ms timer cycles since start
 */
uint64_t ESP32_Timers_Get1msCycles(void);

/**
 * @brief Get DMA timer cycle count
 * 
 * @return Number of DMA timer cycles since start
 */
uint64_t ESP32_Timers_GetDMACycles(void);

/**
 * @brief Check if 1ms timer is running
 * 
 * @return true if running, false otherwise
 */
bool ESP32_Timers_Is1msRunning(void);

/**
 * @brief Check if DMA timer is running
 * 
 * @return true if running, false otherwise
 */
bool ESP32_Timers_IsDMARunning(void);

#ifdef __cplusplus
}
#endif

#endif // ESP32_TIMERS_H
