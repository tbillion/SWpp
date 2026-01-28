/**
 * @file esp32_uart.c
 * @brief UART Hardware Abstraction Layer Implementation
 * 
 * Implements hardware UART0 and UART1 with circular buffers and ISR handling.
 * 
 * Contract Compliance:
 * - Section 4.1: Hardware UART peripherals only
 * - Section 4.2: 115200 default, 9600-1000000 range
 * - Section 2: Hardware peripherals, no bit-banging
 * 
 * @author Serial Wombat ESP32-S3 Port
 * @date 2026-01-28
 */

#include "esp32_uart.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "ESP32_UART";

// UART state structure
typedef struct {
    bool initialized;
    ESP32_UART_Stats_t stats;
    ESP32_UART_RxCallback_t rx_callback;
    QueueHandle_t uart_queue;
} ESP32_UART_State_t;

// UART states (one per UART)
static ESP32_UART_State_t uart_states[UART_NUM_MAX] = {0};

// UART event task handle
static TaskHandle_t uart_event_task_handle = NULL;

/**
 * @brief UART event task
 * 
 * Processes UART events from the event queue.
 * Handles RX data, errors, and other events.
 */
static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    
    while (1) {
        // Wait for UART events from both UARTs
        for (uart_port_t uart_num = UART_NUM_0; uart_num < UART_NUM_MAX; uart_num++) {
            if (!uart_states[uart_num].initialized) {
                continue;
            }
            
            if (uart_states[uart_num].uart_queue == NULL) {
                continue;
            }
            
            // Check for events (non-blocking)
            if (xQueueReceive(uart_states[uart_num].uart_queue, &event, 0)) {
                switch (event.type) {
                    case UART_DATA:
                        // Data received
                        uart_states[uart_num].stats.rx_bytes += event.size;
                        
                        // Call callback if registered
                        if (uart_states[uart_num].rx_callback) {
                            uart_states[uart_num].rx_callback(uart_num, event.size);
                        }
                        break;
                        
                    case UART_FIFO_OVF:
                        // FIFO overflow
                        uart_states[uart_num].stats.rx_overflows++;
                        ESP_LOGW(TAG, "UART%d: FIFO overflow", uart_num);
                        uart_flush_input(uart_num);
                        xQueueReset(uart_states[uart_num].uart_queue);
                        break;
                        
                    case UART_BUFFER_FULL:
                        // Ring buffer full
                        uart_states[uart_num].stats.rx_overflows++;
                        ESP_LOGW(TAG, "UART%d: Ring buffer full", uart_num);
                        uart_flush_input(uart_num);
                        xQueueReset(uart_states[uart_num].uart_queue);
                        break;
                        
                    case UART_PARITY_ERR:
                        uart_states[uart_num].stats.parity_errors++;
                        ESP_LOGW(TAG, "UART%d: Parity error", uart_num);
                        break;
                        
                    case UART_FRAME_ERR:
                        uart_states[uart_num].stats.frame_errors++;
                        ESP_LOGW(TAG, "UART%d: Frame error", uart_num);
                        break;
                        
                    case UART_BREAK:
                        uart_states[uart_num].stats.break_conditions++;
                        ESP_LOGD(TAG, "UART%d: Break condition", uart_num);
                        break;
                        
                    default:
                        ESP_LOGD(TAG, "UART%d: Event type %d", uart_num, event.type);
                        break;
                }
            }
        }
        
        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Initialize a UART port with given configuration
 */
static esp_err_t uart_init_port(uart_port_t uart_num, uint32_t baud_rate, 
                                 int tx_pin, int rx_pin, const ESP32_UART_Config_t *config)
{
    if (uart_num >= UART_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid UART number: %d", uart_num);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate baud rate
    if (baud_rate < ESP32_UART_BAUD_MIN || baud_rate > ESP32_UART_BAUD_MAX) {
        ESP_LOGW(TAG, "UART%d: Baud rate %lu out of range, using default", uart_num, baud_rate);
        baud_rate = ESP32_UART_DEFAULT_BAUD;
    }
    
    // Set up default configuration
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // Override with custom configuration if provided
    if (config) {
        uart_config.data_bits = config->data_bits;
        uart_config.parity = config->parity;
        uart_config.stop_bits = config->stop_bits;
        uart_config.flow_ctrl = config->flow_ctrl;
        uart_config.rx_flow_ctrl_thresh = config->rx_flow_ctrl_thresh;
    }
    
    // Configure UART parameters
    esp_err_t ret = uart_param_config(uart_num, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART%d: Failed to configure parameters: %s", uart_num, esp_err_to_name(ret));
        return ret;
    }
    
    // Set UART pins
    ret = uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART%d: Failed to set pins (TX=%d, RX=%d): %s", 
                 uart_num, tx_pin, rx_pin, esp_err_to_name(ret));
        return ret;
    }
    
    // Install UART driver with event queue
    ret = uart_driver_install(uart_num, ESP32_UART_RX_BUF_SIZE, ESP32_UART_TX_BUF_SIZE, 
                               20, &uart_states[uart_num].uart_queue, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART%d: Failed to install driver: %s", uart_num, esp_err_to_name(ret));
        return ret;
    }
    
    // Mark as initialized
    uart_states[uart_num].initialized = true;
    memset(&uart_states[uart_num].stats, 0, sizeof(ESP32_UART_Stats_t));
    
    ESP_LOGI(TAG, "UART%d initialized: TX=%d, RX=%d, Baud=%lu", uart_num, tx_pin, rx_pin, baud_rate);
    
    return ESP_OK;
}

esp_err_t ESP32_UART_Init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing UART subsystem");
    
    // Initialize UART0 (console/setup)
    ret = uart_init_port(ESP32_UART0, ESP32_UART_DEFAULT_BAUD, 
                         ESP32_UART0_TX_PIN, ESP32_UART0_RX_PIN, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UART0");
        return ret;
    }
    
    // Initialize UART1 (protocol transport)
    ret = uart_init_port(ESP32_UART1, ESP32_UART_DEFAULT_BAUD,
                         ESP32_UART1_TX_PIN, ESP32_UART1_RX_PIN, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UART1");
        return ret;
    }
    
    // Create UART event task if not already created
    if (uart_event_task_handle == NULL) {
        xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, &uart_event_task_handle);
        if (uart_event_task_handle == NULL) {
            ESP_LOGE(TAG, "Failed to create UART event task");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "UART event task created");
    }
    
    ESP_LOGI(TAG, "UART subsystem initialized successfully");
    
    return ESP_OK;
}

esp_err_t ESP32_UART_InitUART0(uint32_t baud_rate, const ESP32_UART_Config_t *config)
{
    return uart_init_port(ESP32_UART0, baud_rate, ESP32_UART0_TX_PIN, ESP32_UART0_RX_PIN, config);
}

esp_err_t ESP32_UART_InitUART1(uint32_t baud_rate, const ESP32_UART_Config_t *config)
{
    return uart_init_port(ESP32_UART1, baud_rate, ESP32_UART1_TX_PIN, ESP32_UART1_RX_PIN, config);
}

size_t ESP32_UART_Write(uart_port_t uart_num, const uint8_t *data, size_t length)
{
    if (uart_num >= UART_NUM_MAX || !uart_states[uart_num].initialized) {
        return 0;
    }
    
    if (data == NULL || length == 0) {
        return 0;
    }
    
    int written = uart_write_bytes(uart_num, (const char *)data, length);
    if (written > 0) {
        uart_states[uart_num].stats.tx_bytes += written;
    }
    
    return (written > 0) ? written : 0;
}

size_t ESP32_UART_WriteString(uart_port_t uart_num, const char *str)
{
    if (str == NULL) {
        return 0;
    }
    
    return ESP32_UART_Write(uart_num, (const uint8_t *)str, strlen(str));
}

bool ESP32_UART_WriteByte(uart_port_t uart_num, uint8_t byte)
{
    return ESP32_UART_Write(uart_num, &byte, 1) == 1;
}

size_t ESP32_UART_Read(uart_port_t uart_num, uint8_t *buffer, size_t length)
{
    if (uart_num >= UART_NUM_MAX || !uart_states[uart_num].initialized) {
        return 0;
    }
    
    if (buffer == NULL || length == 0) {
        return 0;
    }
    
    int read = uart_read_bytes(uart_num, buffer, length, 0);
    return (read > 0) ? read : 0;
}

int ESP32_UART_ReadByte(uart_port_t uart_num)
{
    uint8_t byte;
    
    if (ESP32_UART_Read(uart_num, &byte, 1) == 1) {
        return byte;
    }
    
    return -1;
}

size_t ESP32_UART_Available(uart_port_t uart_num)
{
    if (uart_num >= UART_NUM_MAX || !uart_states[uart_num].initialized) {
        return 0;
    }
    
    size_t available = 0;
    uart_get_buffered_data_len(uart_num, &available);
    return available;
}

bool ESP32_UART_HasData(uart_port_t uart_num)
{
    return ESP32_UART_Available(uart_num) > 0;
}

esp_err_t ESP32_UART_Flush(uart_port_t uart_num)
{
    if (uart_num >= UART_NUM_MAX || !uart_states[uart_num].initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return uart_wait_tx_done(uart_num, pdMS_TO_TICKS(1000));
}

esp_err_t ESP32_UART_ClearRX(uart_port_t uart_num)
{
    if (uart_num >= UART_NUM_MAX || !uart_states[uart_num].initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return uart_flush_input(uart_num);
}

esp_err_t ESP32_UART_GetStats(uart_port_t uart_num, ESP32_UART_Stats_t *stats)
{
    if (uart_num >= UART_NUM_MAX || !uart_states[uart_num].initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(stats, &uart_states[uart_num].stats, sizeof(ESP32_UART_Stats_t));
    return ESP_OK;
}

esp_err_t ESP32_UART_ResetStats(uart_port_t uart_num)
{
    if (uart_num >= UART_NUM_MAX || !uart_states[uart_num].initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(&uart_states[uart_num].stats, 0, sizeof(ESP32_UART_Stats_t));
    return ESP_OK;
}

bool ESP32_UART_IsInitialized(uart_port_t uart_num)
{
    if (uart_num >= UART_NUM_MAX) {
        return false;
    }
    
    return uart_states[uart_num].initialized;
}

esp_err_t ESP32_UART_SetRxCallback(uart_port_t uart_num, ESP32_UART_RxCallback_t callback)
{
    if (uart_num >= UART_NUM_MAX || !uart_states[uart_num].initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uart_states[uart_num].rx_callback = callback;
    ESP_LOGI(TAG, "UART%d: RX callback %s", uart_num, callback ? "set" : "cleared");
    
    return ESP_OK;
}
