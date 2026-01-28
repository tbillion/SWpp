/**
 * @file protocol.c
 * @brief Serial Wombat Protocol Implementation
 * 
 * Implements the Serial Wombat 8-byte packet protocol with command dispatching
 * and response generation.
 * 
 * Port: ESP32-S3
 * Phase: 3 (Core Firmware Porting)
 */

#include "protocol.h"
#include "pinRegisters.h"
#include "../hw_abstraction/esp32_uart.h"
#include "../hw_abstraction/esp32_i2c.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Packet buffers
static uint8_t Rxbuffer[SW_PACKET_SIZE];        // Receive buffer
static uint8_t Txbuffer[SW_PACKET_SIZE];        // Transmit buffer

// Protocol state
static bool Initialized = false;
static bool ResponseReady = false;

// Statistics
static SW_Protocol_Stats_t Stats = {0};

// ============================================================================
// INITIALIZATION
// ============================================================================

bool SW_Protocol_Init(void) {
    if (Initialized) {
        return true;  // Already initialized
    }
    
    // Clear buffers
    SW_Protocol_ClearBuffers();
    
    // Reset statistics
    SW_Protocol_ResetStats();
    
    // Mark as initialized
    Initialized = true;
    ResponseReady = false;
    
    printf("[Protocol] Initialized\n");
    return true;
}

void SW_Protocol_Deinit(void) {
    if (!Initialized) {
        return;
    }
    
    Initialized = false;
    ResponseReady = false;
    
    printf("[Protocol] Deinitialized\n");
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void uint16ToAscii5(uint16_t value, uint8_t* buffer) {
    // Convert uint16 to 5-digit ASCII (00000-99999)
    if (value > 99999) value = 99999;
    
    buffer[4] = '0' + (value % 10); value /= 10;
    buffer[3] = '0' + (value % 10); value /= 10;
    buffer[2] = '0' + (value % 10); value /= 10;
    buffer[1] = '0' + (value % 10); value /= 10;
    buffer[0] = '0' + (value % 10);
}

// ============================================================================
// COMMAND HANDLERS (Placeholders for Phase 3 Step 4+)
// ============================================================================

static void ProcessPinModeCommand(uint8_t command) {
    // Pin mode configuration: command byte IS the mode
    // Rxbuffer[1-7] contains the configuration for this mode
    uint8_t pin = Rxbuffer[1];  // Pin number is first config byte
    uint8_t mode = command;      // Command byte is the mode
    
    // Set pin mode with configuration
    if (SW_PinRegisters_SetMode(pin, mode, &Rxbuffer[1]) == SW_OK) {
        // Success - echo command and return pin info
        Txbuffer[0] = command;
        Txbuffer[1] = pin;
        Txbuffer[2] = mode;
        // Pad remaining bytes
        for (int i = 3; i < SW_PACKET_SIZE; i++) {
            Txbuffer[i] = SW_RESYNC_CHAR;
        }
        printf("[Protocol] Set pin %d to mode 0x%02X\n", pin, mode);
    } else {
        SW_Protocol_SendError(SW_ERROR_INVALID_PIN);
        Stats.invalid_commands++;
    }
}

static void ProcessReadPublicData(uint8_t command) {
    // Read public data from pin's buffer
    // Command: 0x81-0x8F (pin 1-15)
    uint8_t pin = command - READ_PUBLIC_DATA_0;
    uint16_t index = (Rxbuffer[1] << 8) | Rxbuffer[2];
    uint8_t data[5];  // Read 5 bytes
    
    if (SW_PinRegisters_ReadPublicData(pin, index, data, 5) == SW_OK) {
        // Success - return data
        Txbuffer[0] = command;
        memcpy(&Txbuffer[1], data, 5);
        // Pad remaining bytes
        Txbuffer[6] = SW_RESYNC_CHAR;
        Txbuffer[7] = SW_RESYNC_CHAR;
        printf("[Protocol] Read pin %d data[%d]: %02X %02X %02X %02X %02X\n", 
               pin, index, data[0], data[1], data[2], data[3], data[4]);
    } else {
        SW_Protocol_SendError(SW_ERROR_INVALID_PIN);
        Stats.invalid_commands++;
    }
}

static void ProcessWritePublicData(uint8_t command) {
    // Write public data to pin's buffer
    // Command: 0x91-0x9F (pin 1-15)
    uint8_t pin = command - WRITE_PUBLIC_DATA_0;
    uint16_t index = (Rxbuffer[1] << 8) | Rxbuffer[2];
    const uint8_t* data = &Rxbuffer[3];  // 5 bytes of data
    
    if (SW_PinRegisters_WritePublicData(pin, index, data, 5) == SW_OK) {
        // Success - acknowledge
        Txbuffer[0] = command;
        Txbuffer[1] = Rxbuffer[1];  // Echo index high byte
        Txbuffer[2] = Rxbuffer[2];  // Echo index low byte
        // Pad remaining bytes
        for (int i = 3; i < SW_PACKET_SIZE; i++) {
            Txbuffer[i] = SW_RESYNC_CHAR;
        }
        printf("[Protocol] Write pin %d data[%d]: %02X %02X %02X %02X %02X\n", 
               pin, index, data[0], data[1], data[2], data[3], data[4]);
    } else {
        SW_Protocol_SendError(SW_ERROR_INVALID_PIN);
        Stats.invalid_commands++;
    }
}

static void ProcessSystemCommand(uint8_t command) {
    switch (command) {
        case CMD_VERSION:
            // Return version info: ESP32-S3 v0.1
            Txbuffer[1] = 'E';  // ESP32
            Txbuffer[2] = '3';
            Txbuffer[3] = '2';
            Txbuffer[4] = 'S';
            Txbuffer[5] = '3';
            Txbuffer[6] = 0x00;  // Major version
            Txbuffer[7] = 0x01;  // Minor version
            printf("[Protocol] Version query: ESP32-S3 v0.1\n");
            break;
            
        case CMD_RESET:
            printf("[Protocol] Reset command (acknowledged, not executed)\n");
            for (int i = 1; i < SW_PACKET_SIZE; i++) {
                Txbuffer[i] = SW_RESYNC_CHAR;
            }
            // Note: Actual reset would happen after response sent
            break;
            
        case 0xF8:  // PIN_COUNT command
            Txbuffer[1] = SW_PinRegisters_GetPinCount();  // 22 pins
            for (int i = 2; i < SW_PACKET_SIZE; i++) {
                Txbuffer[i] = SW_RESYNC_CHAR;
            }
            printf("[Protocol] Pin count query: %d pins\n", SW_PinRegisters_GetPinCount());
            break;
            
        default:
            printf("[Protocol] System command 0x%02X (not implemented)\n", command);
            for (int i = 1; i < SW_PACKET_SIZE; i++) {
                Txbuffer[i] = SW_RESYNC_CHAR;
            }
            break;
    }
}

// ============================================================================
// MAIN PACKET PROCESSOR
// ============================================================================

void ProcessRxbuffer(void) {
    uint8_t command = Rxbuffer[0];
    
    // Echo command byte in response
    Txbuffer[0] = command;
    
    // Dispatch based on command range
    if (IS_PIN_MODE_CMD(command)) {
        // Pin mode configuration (0x00-0x7F)
        ProcessPinModeCommand(command);
    }
    else if (IS_READ_DATA_CMD(command)) {
        // Read public data (0x81-0x8F)
        ProcessReadPublicData(command);
    }
    else if (IS_WRITE_DATA_CMD(command)) {
        // Write public data (0x91-0x9F)
        ProcessWritePublicData(command);
    }
    else if (IS_SYSTEM_CMD(command)) {
        // System commands (0xF0-0xFF)
        ProcessSystemCommand(command);
    }
    else {
        // Unknown command
        printf("[Protocol] Unknown command: 0x%02X\n", command);
        SW_Protocol_SendError(SW_ERROR_INVALID_COMMAND);
        Stats.invalid_commands++;
        return;
    }
    
    // Mark response as ready
    ResponseReady = true;
    Stats.packets_processed++;
}

// ============================================================================
// PACKET PROCESSING API
// ============================================================================

bool SW_Protocol_ProcessRxPacket(const uint8_t* packet) {
    if (!Initialized || !packet) {
        return false;
    }
    
    // Copy packet to RX buffer
    memcpy(Rxbuffer, packet, SW_PACKET_SIZE);
    Stats.packets_received++;
    
    // Process packet
    ProcessRxbuffer();
    
    return ResponseReady;
}

bool SW_Protocol_GetTxPacket(uint8_t* packet) {
    if (!packet || !ResponseReady) {
        return false;
    }
    
    // Copy TX buffer to output
    memcpy(packet, Txbuffer, SW_PACKET_SIZE);
    
    // Clear response ready flag
    ResponseReady = false;
    
    return true;
}

// ============================================================================
// HAL INTEGRATION CALLBACKS
// ============================================================================

void SW_Protocol_OnUARTPacket(const uint8_t* packet, size_t len) {
    if (len != SW_PACKET_SIZE) {
        printf("[Protocol] Invalid UART packet size: %d\n", len);
        return;
    }
    
    Stats.uart_packets++;
    
    // Process packet
    if (SW_Protocol_ProcessRxPacket(packet)) {
        // Send response immediately via UART
        ESP32_UART_Write(UART_NUM_0, Txbuffer, SW_PACKET_SIZE);
    }
}

void SW_Protocol_OnI2CPacket(const uint8_t* packet, size_t len) {
    if (len != SW_PACKET_SIZE) {
        printf("[Protocol] Invalid I2C packet size: %d\n", len);
        return;
    }
    
    Stats.i2c_packets++;
    
    // Process packet
    // Response will be read by I2C master on next read
    SW_Protocol_ProcessRxPacket(packet);
    
    // I2C HAL will read Txbuffer when master reads
}

// ============================================================================
// BUFFER ACCESS
// ============================================================================

uint8_t* SW_Protocol_GetRxBuffer(void) {
    return Rxbuffer;
}

uint8_t* SW_Protocol_GetTxBuffer(void) {
    return Txbuffer;
}

void SW_Protocol_ClearBuffers(void) {
    // Fill with resync character
    memset(Rxbuffer, SW_RESYNC_CHAR, SW_PACKET_SIZE);
    memset(Txbuffer, SW_RESYNC_CHAR, SW_PACKET_SIZE);
    ResponseReady = false;
}

// ============================================================================
// STATUS AND STATISTICS
// ============================================================================

void SW_Protocol_GetStats(SW_Protocol_Stats_t* stats) {
    if (stats) {
        memcpy(stats, &Stats, sizeof(SW_Protocol_Stats_t));
    }
}

void SW_Protocol_ResetStats(void) {
    memset(&Stats, 0, sizeof(SW_Protocol_Stats_t));
}

bool SW_Protocol_IsResponseReady(void) {
    return ResponseReady;
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

void SW_Protocol_SendError(SW_ERROR_t errorCode) {
    // Format: ['E', '0', '0', '0', 'X', 'X', 0x55, 0x55]
    Txbuffer[0] = 'E';
    uint16ToAscii5((uint16_t)errorCode, &Txbuffer[1]);
    Txbuffer[6] = SW_RESYNC_CHAR;
    Txbuffer[7] = SW_RESYNC_CHAR;
    
    ResponseReady = true;
    Stats.errors++;
    Stats.last_error = errorCode;
    
    printf("[Protocol] Error: %d\n", errorCode);
}

SW_ERROR_t SW_Protocol_GetLastError(void) {
    return Stats.last_error;
}
