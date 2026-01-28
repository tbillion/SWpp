/**
 * @file protocol.h
 * @brief Serial Wombat Protocol Handler
 * 
 * This file implements the Serial Wombat 8-byte packet protocol.
 * It provides packet processing, command dispatching, and response generation.
 * 
 * Protocol Structure:
 * - Packet size: 8 bytes (fixed)
 * - Command byte: Rxbuffer[0]
 * - Parameters: Rxbuffer[1-7]
 * - Response: Txbuffer[0-7] (command echoed in byte 0)
 * 
 * Port: ESP32-S3
 * Phase: 3 (Core Firmware Porting)
 * Contract: 100% protocol compatibility with SW18AB
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "commands.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CONSTANTS
// ============================================================================

#define SW_PACKET_SIZE                  8       // Fixed 8-byte packets
#define SW_RESYNC_CHAR                  0x55    // Resync/padding character
#define SW_MAX_PINS                     22      // ESP32-S3 pin count

// ============================================================================
// PACKET STRUCTURES
// ============================================================================

/**
 * @brief Serial Wombat packet structure
 */
typedef struct {
    uint8_t data[SW_PACKET_SIZE];               // 8-byte packet data
} SW_Packet_t;

/**
 * @brief Protocol statistics
 */
typedef struct {
    uint32_t packets_received;                  // Total packets received
    uint32_t packets_processed;                 // Total packets processed successfully
    uint32_t errors;                            // Total errors encountered
    uint32_t invalid_commands;                  // Unknown command count
    uint32_t uart_packets;                      // Packets from UART
    uint32_t i2c_packets;                       // Packets from I2C
    SW_ERROR_t last_error;                      // Last error code
} SW_Protocol_Stats_t;

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize the protocol handler
 * 
 * Sets up buffers, statistics, and prepares for packet processing.
 * 
 * @return true if successful, false otherwise
 */
bool SW_Protocol_Init(void);

/**
 * @brief Deinitialize the protocol handler
 * 
 * Cleans up resources and resets state.
 */
void SW_Protocol_Deinit(void);

// ============================================================================
// PACKET PROCESSING
// ============================================================================

/**
 * @brief Process a received packet
 * 
 * Main entry point for packet processing. Dispatches to appropriate
 * command handler and generates response in Txbuffer.
 * 
 * @param packet Pointer to 8-byte received packet
 * @return true if processed successfully, false if error
 */
bool SW_Protocol_ProcessRxPacket(const uint8_t* packet);

/**
 * @brief Get the response packet
 * 
 * Retrieves the generated response after processing.
 * 
 * @param packet Pointer to buffer for 8-byte response
 * @return true if response is ready, false otherwise
 */
bool SW_Protocol_GetTxPacket(uint8_t* packet);

// ============================================================================
// CALLBACK HANDLERS (HAL Integration)
// ============================================================================

/**
 * @brief UART packet received callback
 * 
 * Called by UART HAL when a complete 8-byte packet is received.
 * Processes packet and sends response automatically.
 * 
 * @param packet Pointer to received 8-byte packet
 * @param len Length of packet (should be 8)
 */
void SW_Protocol_OnUARTPacket(const uint8_t* packet, size_t len);

/**
 * @brief I2C packet received callback
 * 
 * Called by I2C HAL when master writes 8-byte packet.
 * Processes packet and prepares response for next read.
 * 
 * @param packet Pointer to received 8-byte packet
 * @param len Length of packet (should be 8)
 */
void SW_Protocol_OnI2CPacket(const uint8_t* packet, size_t len);

// ============================================================================
// BUFFER ACCESS
// ============================================================================

/**
 * @brief Get pointer to RX buffer
 * 
 * Direct access to receive buffer for advanced use.
 * 
 * @return Pointer to 8-byte RX buffer
 */
uint8_t* SW_Protocol_GetRxBuffer(void);

/**
 * @brief Get pointer to TX buffer
 * 
 * Direct access to transmit buffer for advanced use.
 * 
 * @return Pointer to 8-byte TX buffer
 */
uint8_t* SW_Protocol_GetTxBuffer(void);

/**
 * @brief Clear RX and TX buffers
 * 
 * Resets both buffers to resync character (0x55).
 */
void SW_Protocol_ClearBuffers(void);

// ============================================================================
// STATUS AND STATISTICS
// ============================================================================

/**
 * @brief Get protocol statistics
 * 
 * @param stats Pointer to statistics structure to fill
 */
void SW_Protocol_GetStats(SW_Protocol_Stats_t* stats);

/**
 * @brief Reset protocol statistics
 * 
 * Clears all counters to zero.
 */
void SW_Protocol_ResetStats(void);

/**
 * @brief Check if response is ready
 * 
 * @return true if TX buffer contains valid response
 */
bool SW_Protocol_IsResponseReady(void);

// ============================================================================
// ERROR HANDLING
// ============================================================================

/**
 * @brief Send error response
 * 
 * Formats TX buffer with error code in ASCII format:
 * ['E', '0', '0', '0', 'X', 'X', 0x55, 0x55]
 * 
 * @param errorCode Error code to send
 */
void SW_Protocol_SendError(SW_ERROR_t errorCode);

/**
 * @brief Get last error code
 * 
 * @return Last error that occurred
 */
SW_ERROR_t SW_Protocol_GetLastError(void);

// ============================================================================
// INTERNAL FUNCTIONS (declared for testing, not part of public API)
// ============================================================================

/**
 * @brief Process received buffer (main dispatcher)
 * 
 * Internal function that dispatches commands to handlers.
 * Called by ProcessRxPacket after validation.
 */
void ProcessRxbuffer(void);

/**
 * @brief Convert uint16 to 5-digit ASCII
 * 
 * Helper for error code formatting.
 * 
 * @param value Value to convert (0-99999)
 * @param buffer Pointer to 5-byte buffer for ASCII digits
 */
void uint16ToAscii5(uint16_t value, uint8_t* buffer);

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_H
