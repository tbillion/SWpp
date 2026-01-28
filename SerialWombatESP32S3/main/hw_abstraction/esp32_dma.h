/**
 * @file esp32_dma.h
 * @brief ESP32-S3 DMA (High-Speed GPIO Sampling) Abstraction Layer
 * 
 * Provides high-speed GPIO state capture for Serial Wombat protocol.
 * Software baseline implementation (Tier 1) for 57.6kHz sampling rate.
 * 
 * Contract Compliance:
 * - Section 7.1: Performance equivalent to SW18AB (57.6kHz)
 * - Section 7.2: Software baseline, bounded ISR time
 * - Section 7.3: Stable default configuration
 * 
 * @author Serial Wombat ESP32-S3 Port
 * @date 2026-01-28
 */

#ifndef ESP32_DMA_H
#define ESP32_DMA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DMA sample structure
 * 
 * Contains GPIO state for all 22 pins captured at a specific timestamp.
 * Bit field format: bit 0 = pin 0, bit 1 = pin 1, ... bit 21 = pin 21.
 */
typedef struct {
    uint32_t gpio_state;        ///< Bit field of all 22 GPIO pins (bits 0-21 used)
    uint64_t timestamp_us;      ///< Microsecond timestamp when captured
} ESP32_DMA_Sample_t;

/**
 * @brief DMA configuration structure
 */
typedef struct {
    uint32_t buffer_size;       ///< Number of samples in circular buffer (256-8192)
    bool auto_start;            ///< Start sampling immediately after init
} ESP32_DMA_Config_t;

/**
 * @brief DMA statistics structure
 */
typedef struct {
    uint64_t samples_captured;  ///< Total samples captured since init
    uint64_t samples_read;      ///< Total samples read by application
    uint64_t buffer_overflows;  ///< Number of times buffer wrapped (oldest data lost)
    uint32_t buffer_occupancy;  ///< Current number of samples in buffer
    uint32_t buffer_capacity;   ///< Maximum buffer capacity
    float sample_rate_hz;       ///< Actual sampling rate (Hz)
    bool is_running;            ///< Currently sampling
} ESP32_DMA_Stats_t;

/**
 * @brief Default DMA configuration
 */
#define ESP32_DMA_DEFAULT_CONFIG() { \
    .buffer_size = 1024,            \
    .auto_start = false             \
}

/**
 * @brief Initialize DMA subsystem
 * 
 * @param config Configuration struct (NULL for defaults)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_DMA_Init(const ESP32_DMA_Config_t *config);

/**
 * @brief Deinitialize DMA subsystem
 * 
 * Stops sampling and frees resources.
 * 
 * @return ESP_OK on success
 */
esp_err_t ESP32_DMA_Deinit(void);

/**
 * @brief Check if DMA subsystem is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool ESP32_DMA_IsInitialized(void);

/**
 * @brief Start GPIO sampling
 * 
 * Begins capturing GPIO states at 57.6kHz into circular buffer.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_DMA_Start(void);

/**
 * @brief Stop GPIO sampling
 * 
 * @return ESP_OK on success
 */
esp_err_t ESP32_DMA_Stop(void);

/**
 * @brief Check if sampling is running
 * 
 * @return true if sampling, false otherwise
 */
bool ESP32_DMA_IsRunning(void);

/**
 * @brief Read a single sample from buffer
 * 
 * Removes sample from buffer (FIFO behavior).
 * 
 * @param sample Pointer to receive sample data
 * @return ESP_OK if sample read, ESP_ERR_NOT_FOUND if buffer empty
 */
esp_err_t ESP32_DMA_Read(ESP32_DMA_Sample_t *sample);

/**
 * @brief Read multiple samples from buffer
 * 
 * Reads up to max_samples from buffer.
 * 
 * @param samples Array to receive samples
 * @param max_samples Maximum number of samples to read
 * @param samples_read Pointer to receive actual number read (can be NULL)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_DMA_ReadMultiple(ESP32_DMA_Sample_t *samples, size_t max_samples, size_t *samples_read);

/**
 * @brief Peek at next sample without removing from buffer
 * 
 * @param sample Pointer to receive sample data
 * @return ESP_OK if sample available, ESP_ERR_NOT_FOUND if buffer empty
 */
esp_err_t ESP32_DMA_Peek(ESP32_DMA_Sample_t *sample);

/**
 * @brief Get number of samples available in buffer
 * 
 * @return Number of samples available to read
 */
size_t ESP32_DMA_Available(void);

/**
 * @brief Clear all samples from buffer
 * 
 * @return ESP_OK on success
 */
esp_err_t ESP32_DMA_Clear(void);

/**
 * @brief Check if buffer is full
 * 
 * @return true if buffer is full, false otherwise
 */
bool ESP32_DMA_IsFull(void);

/**
 * @brief Check if buffer is empty
 * 
 * @return true if buffer is empty, false otherwise
 */
bool ESP32_DMA_IsEmpty(void);

/**
 * @brief Get buffer capacity
 * 
 * @return Maximum number of samples buffer can hold
 */
size_t ESP32_DMA_GetCapacity(void);

/**
 * @brief Get DMA statistics
 * 
 * @param stats Pointer to receive statistics
 * @return ESP_OK on success
 */
esp_err_t ESP32_DMA_GetStats(ESP32_DMA_Stats_t *stats);

/**
 * @brief Reset statistics counters
 * 
 * Resets captured/read/overflow counters to zero.
 * Does not clear buffer or stop sampling.
 * 
 * @return ESP_OK on success
 */
esp_err_t ESP32_DMA_ResetStats(void);

/**
 * @brief Get actual sampling rate
 * 
 * Calculates rate based on timestamp deltas.
 * 
 * @return Sampling rate in Hz (e.g., 57600.0)
 */
float ESP32_DMA_GetSampleRate(void);

/**
 * @brief Get buffer usage percentage
 * 
 * @return Percentage of buffer full (0.0-100.0)
 */
float ESP32_DMA_GetBufferUsage(void);

/**
 * @brief Extract pin state from sample
 * 
 * Helper function to get state of specific pin from sample.
 * 
 * @param sample Pointer to sample
 * @param pin Pin number (0-21)
 * @return Pin state (0 or 1), or 0 if invalid pin
 */
uint8_t ESP32_DMA_GetPinState(const ESP32_DMA_Sample_t *sample, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif // ESP32_DMA_H
