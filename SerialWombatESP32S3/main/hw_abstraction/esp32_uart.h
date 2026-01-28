/**
 * @file esp32_uart.h
 * @brief UART Hardware Abstraction Layer for Serial Wombat ESP32-S3 Port
 * 
 * Provides abstraction for UART0 (console/setup) and UART1 (protocol transport).
 * Implements hardware UART with circular buffers per contract requirements.
 * 
 * Contract Compliance:
 * - Section 4.1: Hardware UART only (no bit-banging)
 * - Section 4.2: Default 115200, runtime configurable
 * - Pin safety: GPIO 43/44 (UART0), GPIO 47/48 (UART1)
 * 
 * @author Serial Wombat ESP32-S3 Port
 * @date 2026-01-28
 */

#ifndef ESP32_UART_H
#define ESP32_UART_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

// UART pin definitions (from PIN_MAPPING.md Section 7.3)
#define ESP32_UART0_TX_PIN      43  // UART0 TX (console/setup)
#define ESP32_UART0_RX_PIN      44  // UART0 RX (console/setup)
#define ESP32_UART1_TX_PIN      47  // UART1 TX (protocol transport)
#define ESP32_UART1_RX_PIN      48  // UART1 RX (protocol transport)

// UART numbers
#define ESP32_UART0             UART_NUM_0
#define ESP32_UART1             UART_NUM_1

// Default configuration
#define ESP32_UART_DEFAULT_BAUD 115200
#define ESP32_UART_RX_BUF_SIZE  1024    // Ring buffer size for RX
#define ESP32_UART_TX_BUF_SIZE  512     // Ring buffer size for TX

// Baud rate limits (per contract: 9600 to 1000000)
#define ESP32_UART_BAUD_MIN     9600
#define ESP32_UART_BAUD_MAX     1000000

/**
 * @brief UART configuration structure
 */
typedef struct {
    uint32_t baud_rate;             // Baud rate (9600 to 1000000)
    uart_word_length_t data_bits;   // Data bits (5/6/7/8)
    uart_stop_bits_t stop_bits;     // Stop bits (1/1.5/2)
    uart_parity_t parity;           // Parity (None/Even/Odd)
    uart_hw_flowcontrol_t flow_ctrl;// Flow control (None/RTS/CTS/Both)
    uint8_t rx_flow_ctrl_thresh;    // RX flow control threshold
} ESP32_UART_Config_t;

/**
 * @brief UART statistics structure
 */
typedef struct {
    uint32_t tx_bytes;              // Total bytes transmitted
    uint32_t rx_bytes;              // Total bytes received
    uint32_t rx_overflows;          // Buffer overflow count
    uint32_t parity_errors;         // Parity error count
    uint32_t frame_errors;          // Frame error count
    uint32_t break_conditions;      // Break condition count
} ESP32_UART_Stats_t;

/**
 * @brief UART RX callback function type
 * @param uart_num UART number (0 or 1)
 * @param data_available Number of bytes available
 */
typedef void (*ESP32_UART_RxCallback_t)(uart_port_t uart_num, size_t data_available);

/**
 * @brief Initialize UART subsystem
 * 
 * Initializes both UART0 and UART1 with default configuration:
 * - UART0: 115200 baud, 8N1, no flow control (console/setup)
 * - UART1: 115200 baud, 8N1, no flow control (protocol transport)
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_UART_Init(void);

/**
 * @brief Initialize UART0 with custom configuration
 * 
 * @param baud_rate Baud rate (9600 to 1000000)
 * @param config Pointer to configuration structure (NULL for defaults)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_UART_InitUART0(uint32_t baud_rate, const ESP32_UART_Config_t *config);

/**
 * @brief Initialize UART1 with custom configuration
 * 
 * @param baud_rate Baud rate (9600 to 1000000)
 * @param config Pointer to configuration structure (NULL for defaults)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_UART_InitUART1(uint32_t baud_rate, const ESP32_UART_Config_t *config);

/**
 * @brief Write data to UART
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @param data Pointer to data buffer
 * @param length Number of bytes to write
 * @return size_t Number of bytes actually written
 */
size_t ESP32_UART_Write(uart_port_t uart_num, const uint8_t *data, size_t length);

/**
 * @brief Write a null-terminated string to UART
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @param str Null-terminated string
 * @return size_t Number of bytes actually written
 */
size_t ESP32_UART_WriteString(uart_port_t uart_num, const char *str);

/**
 * @brief Write a single byte to UART
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @param byte Byte to write
 * @return bool true if byte was written, false otherwise
 */
bool ESP32_UART_WriteByte(uart_port_t uart_num, uint8_t byte);

/**
 * @brief Read data from UART
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @param buffer Pointer to destination buffer
 * @param length Maximum number of bytes to read
 * @return size_t Number of bytes actually read
 */
size_t ESP32_UART_Read(uart_port_t uart_num, uint8_t *buffer, size_t length);

/**
 * @brief Read a single byte from UART
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @return int Byte value (0-255) on success, -1 if no data available
 */
int ESP32_UART_ReadByte(uart_port_t uart_num);

/**
 * @brief Check number of bytes available in RX buffer
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @return size_t Number of bytes available to read
 */
size_t ESP32_UART_Available(uart_port_t uart_num);

/**
 * @brief Check if data is available in RX buffer
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @return bool true if data is available, false otherwise
 */
bool ESP32_UART_HasData(uart_port_t uart_num);

/**
 * @brief Flush TX buffer (wait for transmission to complete)
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_UART_Flush(uart_port_t uart_num);

/**
 * @brief Clear RX buffer (discard all received data)
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_UART_ClearRX(uart_port_t uart_num);

/**
 * @brief Get UART statistics
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @param stats Pointer to statistics structure
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_UART_GetStats(uart_port_t uart_num, ESP32_UART_Stats_t *stats);

/**
 * @brief Reset UART statistics counters
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_UART_ResetStats(uart_port_t uart_num);

/**
 * @brief Check if UART is initialized
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @return bool true if initialized, false otherwise
 */
bool ESP32_UART_IsInitialized(uart_port_t uart_num);

/**
 * @brief Set RX callback function
 * 
 * Callback is called from UART event task when data is received.
 * Keep callback short and fast.
 * 
 * @param uart_num UART number (UART_NUM_0 or UART_NUM_1)
 * @param callback Callback function (NULL to disable)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t ESP32_UART_SetRxCallback(uart_port_t uart_num, ESP32_UART_RxCallback_t callback);

#ifdef __cplusplus
}
#endif

#endif // ESP32_UART_H
