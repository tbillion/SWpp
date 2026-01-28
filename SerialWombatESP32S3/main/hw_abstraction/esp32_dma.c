/**
 * @file esp32_dma.c
 * @brief ESP32-S3 DMA (High-Speed GPIO Sampling) Implementation
 * 
 * Software baseline implementation (Tier 1) using timer-triggered GPIO reads.
 * Integrates with 57.6kHz timer (esp32_timers.c) for sampling trigger.
 */

#include "esp32_dma.h"
#include "esp32_gpio.h"
#include "esp32_timers.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ESP32_DMA";

// DMA state structure
typedef struct {
    bool initialized;
    bool running;
    
    // Circular buffer
    ESP32_DMA_Sample_t *buffer;
    uint32_t buffer_size;
    volatile uint32_t head;  // Write position (ISR writes here)
    volatile uint32_t tail;  // Read position (application reads here)
    
    // Statistics
    uint64_t samples_captured;
    uint64_t samples_read;
    uint64_t buffer_overflows;
    uint64_t last_sample_time_us;
    uint64_t first_sample_time_us;
} ESP32_DMA_State_t;

static ESP32_DMA_State_t dma_state = {
    .initialized = false,
    .running = false,
    .buffer = NULL,
    .buffer_size = 0,
    .head = 0,
    .tail = 0,
    .samples_captured = 0,
    .samples_read = 0,
    .buffer_overflows = 0,
    .last_sample_time_us = 0,
    .first_sample_time_us = 0
};

/**
 * @brief DMA sampling callback (called from 57.6kHz timer ISR)
 * 
 * This function is called approximately every 17µs to sample GPIO states.
 * Must execute quickly (<5µs) to avoid ISR watchdog.
 */
static void IRAM_ATTR dma_sampling_callback(uint64_t timestamp_us)
{
    if (!dma_state.running || !dma_state.buffer) {
        return;
    }
    
    // Read all 22 GPIO pins into bit field
    uint32_t gpio_state = 0;
    for (uint8_t pin = 0; pin < 22; pin++) {
        int level = ESP32_GPIO_Read(pin);
        if (level > 0) {
            gpio_state |= (1 << pin);
        }
    }
    
    // Store sample in circular buffer
    uint32_t next_head = (dma_state.head + 1) % dma_state.buffer_size;
    
    // Check for buffer overflow (head catches up to tail)
    if (next_head == dma_state.tail) {
        dma_state.buffer_overflows++;
        // Advance tail to make room (overwrite oldest data)
        dma_state.tail = (dma_state.tail + 1) % dma_state.buffer_size;
    }
    
    // Write sample
    dma_state.buffer[dma_state.head].gpio_state = gpio_state;
    dma_state.buffer[dma_state.head].timestamp_us = timestamp_us;
    dma_state.head = next_head;
    
    // Update statistics
    dma_state.samples_captured++;
    dma_state.last_sample_time_us = timestamp_us;
    if (dma_state.first_sample_time_us == 0) {
        dma_state.first_sample_time_us = timestamp_us;
    }
}

esp_err_t ESP32_DMA_Init(const ESP32_DMA_Config_t *config)
{
    if (dma_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Use default config if none provided
    ESP32_DMA_Config_t cfg = ESP32_DMA_DEFAULT_CONFIG();
    if (config) {
        cfg = *config;
    }
    
    // Validate buffer size
    if (cfg.buffer_size < 256 || cfg.buffer_size > 8192) {
        ESP_LOGE(TAG, "Invalid buffer size: %lu (must be 256-8192)", cfg.buffer_size);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocate circular buffer
    dma_state.buffer = (ESP32_DMA_Sample_t *)malloc(cfg.buffer_size * sizeof(ESP32_DMA_Sample_t));
    if (!dma_state.buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer (%lu bytes)", 
                 cfg.buffer_size * sizeof(ESP32_DMA_Sample_t));
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize state
    dma_state.buffer_size = cfg.buffer_size;
    dma_state.head = 0;
    dma_state.tail = 0;
    dma_state.samples_captured = 0;
    dma_state.samples_read = 0;
    dma_state.buffer_overflows = 0;
    dma_state.last_sample_time_us = 0;
    dma_state.first_sample_time_us = 0;
    dma_state.running = false;
    dma_state.initialized = true;
    
    ESP_LOGI(TAG, "Initialized with buffer size %lu samples (%lu bytes)",
             cfg.buffer_size, cfg.buffer_size * sizeof(ESP32_DMA_Sample_t));
    
    // Auto-start if requested
    if (cfg.auto_start) {
        return ESP32_DMA_Start();
    }
    
    return ESP_OK;
}

esp_err_t ESP32_DMA_Deinit(void)
{
    if (!dma_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Stop sampling if running
    if (dma_state.running) {
        ESP32_DMA_Stop();
    }
    
    // Free buffer
    if (dma_state.buffer) {
        free(dma_state.buffer);
        dma_state.buffer = NULL;
    }
    
    dma_state.initialized = false;
    dma_state.buffer_size = 0;
    
    ESP_LOGI(TAG, "Deinitialized");
    return ESP_OK;
}

bool ESP32_DMA_IsInitialized(void)
{
    return dma_state.initialized;
}

esp_err_t ESP32_DMA_Start(void)
{
    if (!dma_state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (dma_state.running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }
    
    // Clear buffer and statistics
    dma_state.head = 0;
    dma_state.tail = 0;
    dma_state.samples_captured = 0;
    dma_state.buffer_overflows = 0;
    dma_state.first_sample_time_us = 0;
    dma_state.last_sample_time_us = 0;
    
    // Register callback with DMA timer
    esp_err_t err = ESP32_Timers_StartDMA(dma_sampling_callback);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start DMA timer: %d", err);
        return err;
    }
    
    dma_state.running = true;
    ESP_LOGI(TAG, "Sampling started at 57.6kHz");
    
    return ESP_OK;
}

esp_err_t ESP32_DMA_Stop(void)
{
    if (!dma_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!dma_state.running) {
        return ESP_OK;
    }
    
    // Stop DMA timer
    ESP32_Timers_StopDMA();
    
    dma_state.running = false;
    ESP_LOGI(TAG, "Sampling stopped");
    
    return ESP_OK;
}

bool ESP32_DMA_IsRunning(void)
{
    return dma_state.running;
}

esp_err_t ESP32_DMA_Read(ESP32_DMA_Sample_t *sample)
{
    if (!dma_state.initialized || !sample) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if buffer empty
    if (dma_state.head == dma_state.tail) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read from tail (oldest data first, FIFO)
    taskENTER_CRITICAL();
    *sample = dma_state.buffer[dma_state.tail];
    dma_state.tail = (dma_state.tail + 1) % dma_state.buffer_size;
    dma_state.samples_read++;
    taskEXIT_CRITICAL();
    
    return ESP_OK;
}

esp_err_t ESP32_DMA_ReadMultiple(ESP32_DMA_Sample_t *samples, size_t max_samples, size_t *samples_read)
{
    if (!dma_state.initialized || !samples || max_samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t count = 0;
    while (count < max_samples && dma_state.head != dma_state.tail) {
        taskENTER_CRITICAL();
        samples[count] = dma_state.buffer[dma_state.tail];
        dma_state.tail = (dma_state.tail + 1) % dma_state.buffer_size;
        dma_state.samples_read++;
        taskEXIT_CRITICAL();
        count++;
    }
    
    if (samples_read) {
        *samples_read = count;
    }
    
    return ESP_OK;
}

esp_err_t ESP32_DMA_Peek(ESP32_DMA_Sample_t *sample)
{
    if (!dma_state.initialized || !sample) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if buffer empty
    if (dma_state.head == dma_state.tail) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read without advancing tail
    taskENTER_CRITICAL();
    *sample = dma_state.buffer[dma_state.tail];
    taskEXIT_CRITICAL();
    
    return ESP_OK;
}

size_t ESP32_DMA_Available(void)
{
    if (!dma_state.initialized) {
        return 0;
    }
    
    // Calculate occupied samples
    uint32_t head = dma_state.head;
    uint32_t tail = dma_state.tail;
    
    if (head >= tail) {
        return head - tail;
    } else {
        return (dma_state.buffer_size - tail) + head;
    }
}

esp_err_t ESP32_DMA_Clear(void)
{
    if (!dma_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    taskENTER_CRITICAL();
    dma_state.tail = dma_state.head;
    taskEXIT_CRITICAL();
    
    return ESP_OK;
}

bool ESP32_DMA_IsFull(void)
{
    if (!dma_state.initialized) {
        return false;
    }
    
    uint32_t next_head = (dma_state.head + 1) % dma_state.buffer_size;
    return (next_head == dma_state.tail);
}

bool ESP32_DMA_IsEmpty(void)
{
    return (ESP32_DMA_Available() == 0);
}

size_t ESP32_DMA_GetCapacity(void)
{
    return dma_state.buffer_size;
}

esp_err_t ESP32_DMA_GetStats(ESP32_DMA_Stats_t *stats)
{
    if (!dma_state.initialized || !stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    stats->samples_captured = dma_state.samples_captured;
    stats->samples_read = dma_state.samples_read;
    stats->buffer_overflows = dma_state.buffer_overflows;
    stats->buffer_occupancy = ESP32_DMA_Available();
    stats->buffer_capacity = dma_state.buffer_size;
    stats->sample_rate_hz = ESP32_DMA_GetSampleRate();
    stats->is_running = dma_state.running;
    
    return ESP_OK;
}

esp_err_t ESP32_DMA_ResetStats(void)
{
    if (!dma_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    taskENTER_CRITICAL();
    dma_state.samples_captured = 0;
    dma_state.samples_read = 0;
    dma_state.buffer_overflows = 0;
    dma_state.first_sample_time_us = 0;
    dma_state.last_sample_time_us = 0;
    taskEXIT_CRITICAL();
    
    return ESP_OK;
}

float ESP32_DMA_GetSampleRate(void)
{
    if (!dma_state.initialized || dma_state.samples_captured < 2) {
        return 0.0f;
    }
    
    uint64_t time_diff = dma_state.last_sample_time_us - dma_state.first_sample_time_us;
    if (time_diff == 0) {
        return 0.0f;
    }
    
    // Calculate rate: samples / time_seconds
    float rate = (float)(dma_state.samples_captured - 1) * 1000000.0f / (float)time_diff;
    return rate;
}

float ESP32_DMA_GetBufferUsage(void)
{
    if (!dma_state.initialized || dma_state.buffer_size == 0) {
        return 0.0f;
    }
    
    size_t available = ESP32_DMA_Available();
    return (float)available * 100.0f / (float)dma_state.buffer_size;
}

uint8_t ESP32_DMA_GetPinState(const ESP32_DMA_Sample_t *sample, uint8_t pin)
{
    if (!sample || pin >= 22) {
        return 0;
    }
    
    return (sample->gpio_state & (1 << pin)) ? 1 : 0;
}
