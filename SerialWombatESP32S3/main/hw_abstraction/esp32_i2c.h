/**
 * @file esp32_i2c.h
 * @brief ESP32-S3 I2C Hardware Abstraction Layer
 * 
 * I2C slave implementation for Serial Wombat protocol compatibility.
 * Implements 3-tier approach: Hardware I2C (default), Optimized HW I2C, Software I2C (fallback).
 * 
 * References:
 * - Contract Section 5: I2C requirements
 * - I2C_IMPLEMENTATION.md: Complete design specification
 * - PIN_MAPPING.md Section 7.2: I2C pins (GPIO 8/9)
 * - PIN_MAPPING.md Section 7.4: Address select (GPIO 11-14)
 * 
 * Key Requirements:
 * - Hardware I2C slave mode (Tier 1 default)
 * - Address range: 0x6B-0x7A (16 addresses via GPIO 11-14)
 * - I2C pins: GPIO 8 (SDA), GPIO 9 (SCL)
 * - Packet size: 8 bytes (Serial Wombat protocol)
 * - Clock speed: 100kHz standard mode
 * - Software fallback uses SAME pins (no rewiring)
 */

#ifndef ESP32_I2C_H
#define ESP32_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// I2C Configuration
#define ESP32_I2C_SDA_PIN           8      // GPIO 8 (I2C SDA)
#define ESP32_I2C_SCL_PIN           9      // GPIO 9 (I2C SCL)
#define ESP32_I2C_ADDR_A0_PIN       11     // GPIO 11 (Address bit 0)
#define ESP32_I2C_ADDR_A1_PIN       12     // GPIO 12 (Address bit 1)
#define ESP32_I2C_ADDR_A2_PIN       13     // GPIO 13 (Address bit 2)
#define ESP32_I2C_ADDR_A3_PIN       14     // GPIO 14 (Address bit 3)

#define ESP32_I2C_BASE_ADDRESS      0x6B   // Base address (Serial Wombat default)
#define ESP32_I2C_MAX_ADDRESS       0x7A   // Max address (0x6B + 15)

#define ESP32_I2C_PACKET_SIZE       8      // Serial Wombat packet size
#define ESP32_I2C_RX_BUFFER_SIZE    256    // 32 packets
#define ESP32_I2C_TX_BUFFER_SIZE    256    // 32 packets

#define ESP32_I2C_CLOCK_SPEED       100000 // 100kHz (standard mode, safe)
#define ESP32_I2C_TIMEOUT_MS        100    // I2C operation timeout

// I2C Implementation Tiers
typedef enum {
    ESP32_I2C_TIER_1_HARDWARE = 1,     // Hardware I2C with ESP-IDF driver (default)
    ESP32_I2C_TIER_2_OPTIMIZED = 2,    // Optimized hardware I2C with custom ISR
    ESP32_I2C_TIER_3_SOFTWARE = 3      // Software I2C bit-bang (fallback)
} ESP32_I2C_Tier_t;

// I2C Statistics
typedef struct {
    uint32_t packets_received;         // Total packets received (8-byte)
    uint32_t packets_transmitted;      // Total packets transmitted (8-byte)
    uint32_t incomplete_packets;       // Packets with wrong size
    uint32_t buffer_overflows;         // RX buffer overflow count
    uint32_t ack_errors;               // ACK/NACK errors
    uint32_t bus_errors;               // Bus errors (arbitration, etc.)
    uint32_t timeouts;                 // Operation timeouts
    uint32_t tier_switches;            // Tier fallback count
    ESP32_I2C_Tier_t current_tier;     // Current implementation tier
} ESP32_I2C_Stats_t;

// I2C Packet Callback
// Called when a complete 8-byte packet is received
// rx_packet: Received packet data (8 bytes)
// tx_packet: Buffer to fill with response (8 bytes)
// Returns: true if response ready, false otherwise
typedef bool (*ESP32_I2C_PacketCallback_t)(const uint8_t* rx_packet, uint8_t* tx_packet);

/**
 * @brief Initialize I2C subsystem
 * 
 * Initializes I2C slave mode with hardware I2C (Tier 1 default).
 * Reads address select pins (GPIO 11-14) to determine slave address.
 * 
 * @return true if successful, false otherwise
 */
bool ESP32_I2C_Init(void);

/**
 * @brief Deinitialize I2C subsystem
 * 
 * Stops I2C slave and releases resources.
 */
void ESP32_I2C_Deinit(void);

/**
 * @brief Check if I2C is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool ESP32_I2C_IsInitialized(void);

/**
 * @brief Get current I2C slave address
 * 
 * Address is determined by GPIO 11-14 (A0-A3) at initialization.
 * Address = 0x6B + (A3<<3 | A2<<2 | A1<<1 | A0)
 * 
 * @return Current I2C slave address (0x6B-0x7A)
 */
uint8_t ESP32_I2C_GetAddress(void);

/**
 * @brief Read address select pins
 * 
 * Reads GPIO 11-14 and calculates address offset (0-15).
 * 
 * @return Address offset (0-15)
 */
uint8_t ESP32_I2C_ReadAddressPins(void);

/**
 * @brief Update I2C slave address
 * 
 * Changes the I2C slave address at runtime.
 * Useful for dynamic address configuration.
 * 
 * @param address New slave address (0x6B-0x7A)
 * @return true if successful, false otherwise
 */
bool ESP32_I2C_SetAddress(uint8_t address);

/**
 * @brief Set packet callback
 * 
 * Registers a callback function to handle received packets.
 * The callback is called when a complete 8-byte packet is received.
 * 
 * @param callback Callback function (or NULL to disable)
 */
void ESP32_I2C_SetPacketCallback(ESP32_I2C_PacketCallback_t callback);

/**
 * @brief Get I2C statistics
 * 
 * @param stats Pointer to statistics structure to fill
 */
void ESP32_I2C_GetStats(ESP32_I2C_Stats_t* stats);

/**
 * @brief Reset I2C statistics
 * 
 * Resets all counters to zero (except current tier).
 */
void ESP32_I2C_ResetStats(void);

/**
 * @brief Get current implementation tier
 * 
 * @return Current tier (1=hardware, 2=optimized, 3=software)
 */
ESP32_I2C_Tier_t ESP32_I2C_GetTier(void);

/**
 * @brief Switch to different implementation tier
 * 
 * Allows manual tier selection for testing or when automatic
 * fallback is not desired.
 * 
 * @param tier Target tier (1, 2, or 3)
 * @return true if successful, false otherwise
 */
bool ESP32_I2C_SetTier(ESP32_I2C_Tier_t tier);

/**
 * @brief Enable/disable automatic tier fallback
 * 
 * When enabled, system automatically falls back to lower tier
 * if error rate exceeds threshold (5% errors).
 * 
 * @param enable true to enable, false to disable
 */
void ESP32_I2C_SetAutoFallback(bool enable);

/**
 * @brief Check if automatic fallback is enabled
 * 
 * @return true if enabled, false otherwise
 */
bool ESP32_I2C_IsAutoFallbackEnabled(void);

/**
 * @brief Manual packet write (for testing)
 * 
 * Writes a packet to the TX buffer manually.
 * Normally the packet callback handles this.
 * 
 * @param packet Packet data (8 bytes)
 * @return true if successful, false otherwise
 */
bool ESP32_I2C_WritePacket(const uint8_t* packet);

/**
 * @brief Manual packet read (for testing)
 * 
 * Reads a packet from the RX buffer manually.
 * Normally the packet callback handles this.
 * 
 * @param packet Buffer to store packet (8 bytes)
 * @return true if packet available, false otherwise
 */
bool ESP32_I2C_ReadPacket(uint8_t* packet);

/**
 * @brief Check if packet is available in RX buffer
 * 
 * @return true if packet ready, false otherwise
 */
bool ESP32_I2C_HasPacket(void);

/**
 * @brief Get number of packets in RX buffer
 * 
 * @return Number of complete 8-byte packets available
 */
size_t ESP32_I2C_GetPacketCount(void);

#endif // ESP32_I2C_H
