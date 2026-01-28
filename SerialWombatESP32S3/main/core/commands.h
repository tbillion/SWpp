/**
 * @file commands.h
 * @brief Serial Wombat Protocol Command Definitions
 * 
 * This file contains all command ID definitions from the Serial Wombat 18AB protocol.
 * Commands are organized by category and range for efficient dispatching.
 * 
 * Port: ESP32-S3
 * Phase: 3 (Core Firmware Porting)
 * Contract: 100% command compatibility with SW18AB
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PIN MODE CONFIGURATION COMMANDS (0x00-0xFF)
// ============================================================================
// These commands configure pin modes. The command byte encodes both the mode
// type and the pin number: command = (mode << 4) | pin
// For example: 0x05 = Pin Mode 0, Pin 5

// Pin Mode 0 (Digital I/O) - Commands 0x00-0x0F
#define CONFIGURE_CHANNEL_MODE_0_0      0x00
#define CONFIGURE_CHANNEL_MODE_0_1      0x01
#define CONFIGURE_CHANNEL_MODE_0_2      0x02
#define CONFIGURE_CHANNEL_MODE_0_3      0x03
#define CONFIGURE_CHANNEL_MODE_0_4      0x04
#define CONFIGURE_CHANNEL_MODE_0_5      0x05
#define CONFIGURE_CHANNEL_MODE_0_6      0x06
#define CONFIGURE_CHANNEL_MODE_0_7      0x07
#define CONFIGURE_CHANNEL_MODE_0_8      0x08
#define CONFIGURE_CHANNEL_MODE_0_9      0x09
#define CONFIGURE_CHANNEL_MODE_0_A      0x0A
#define CONFIGURE_CHANNEL_MODE_0_B      0x0B
#define CONFIGURE_CHANNEL_MODE_0_C      0x0C
#define CONFIGURE_CHANNEL_MODE_0_D      0x0D
#define CONFIGURE_CHANNEL_MODE_0_E      0x0E
#define CONFIGURE_CHANNEL_MODE_0_F      0x0F

// Pin Mode 1 (Analog Input) - Commands 0x10-0x1F
#define CONFIGURE_CHANNEL_MODE_1_0      0x10
#define CONFIGURE_CHANNEL_MODE_1_1      0x11
#define CONFIGURE_CHANNEL_MODE_1_2      0x12
#define CONFIGURE_CHANNEL_MODE_1_3      0x13
#define CONFIGURE_CHANNEL_MODE_1_4      0x14
#define CONFIGURE_CHANNEL_MODE_1_5      0x15
#define CONFIGURE_CHANNEL_MODE_1_6      0x16
#define CONFIGURE_CHANNEL_MODE_1_7      0x17
#define CONFIGURE_CHANNEL_MODE_1_8      0x18
#define CONFIGURE_CHANNEL_MODE_1_9      0x19
#define CONFIGURE_CHANNEL_MODE_1_A      0x1A
#define CONFIGURE_CHANNEL_MODE_1_B      0x1B
#define CONFIGURE_CHANNEL_MODE_1_C      0x1C
#define CONFIGURE_CHANNEL_MODE_1_D      0x1D
#define CONFIGURE_CHANNEL_MODE_1_E      0x1E
#define CONFIGURE_CHANNEL_MODE_1_F      0x1F

// Additional pin modes (0x20-0xFF) - 14 more modes × 16 pins = 224 commands
// These would be defined similarly but omitted for brevity
// Pin modes include: PWM, Servo, Debounce, Matrix Keypad, etc.

// ============================================================================
// PIN CONFIGURATION COMMANDS (0x100-0x11F range mapped to available bytes)
// ============================================================================
// Note: In 8-byte protocol, these may use different command bytes

// ============================================================================
// PUBLIC DATA COMMANDS
// ============================================================================

// Read Public Data - Commands 0x81-0x8F (15 pins)
#define READ_PUBLIC_DATA_0              0x81
#define READ_PUBLIC_DATA_1              0x82
#define READ_PUBLIC_DATA_2              0x83
#define READ_PUBLIC_DATA_3              0x84
#define READ_PUBLIC_DATA_4              0x85
#define READ_PUBLIC_DATA_5              0x86
#define READ_PUBLIC_DATA_6              0x87
#define READ_PUBLIC_DATA_7              0x88
#define READ_PUBLIC_DATA_8              0x89
#define READ_PUBLIC_DATA_9              0x8A
#define READ_PUBLIC_DATA_10             0x8B
#define READ_PUBLIC_DATA_11             0x8C
#define READ_PUBLIC_DATA_12             0x8D
#define READ_PUBLIC_DATA_13             0x8E
#define READ_PUBLIC_DATA_14             0x8F

// Write Public Data - Commands 0x91-0x9F (15 pins)
#define WRITE_PUBLIC_DATA_0             0x91
#define WRITE_PUBLIC_DATA_1             0x92
#define WRITE_PUBLIC_DATA_2             0x93
#define WRITE_PUBLIC_DATA_3             0x94
#define WRITE_PUBLIC_DATA_4             0x95
#define WRITE_PUBLIC_DATA_5             0x96
#define WRITE_PUBLIC_DATA_6             0x97
#define WRITE_PUBLIC_DATA_7             0x98
#define WRITE_PUBLIC_DATA_8             0x99
#define WRITE_PUBLIC_DATA_9             0x9A
#define WRITE_PUBLIC_DATA_10            0x9B
#define WRITE_PUBLIC_DATA_11            0x9C
#define WRITE_PUBLIC_DATA_12            0x9D
#define WRITE_PUBLIC_DATA_13            0x9E
#define WRITE_PUBLIC_DATA_14            0x9F

// ============================================================================
// SYSTEM CONFIGURATION COMMANDS
// ============================================================================

// Device Identification
#define CMD_VERSION                     0xFE    // Get firmware version
#define CMD_RESET                       0xFF    // Reset device
#define CMD_UNIQUE_ID_0                 0xF0    // Unique ID byte 0
#define CMD_UNIQUE_ID_1                 0xF1    // Unique ID byte 1
#define CMD_UNIQUE_ID_2                 0xF2    // Unique ID byte 2
#define CMD_UNIQUE_ID_3                 0xF3    // Unique ID byte 3
#define CMD_UNIQUE_ID_4                 0xF4    // Unique ID byte 4
#define CMD_UNIQUE_ID_5                 0xF5    // Unique ID byte 5
#define CMD_UNIQUE_ID_6                 0xF6    // Unique ID byte 6
#define CMD_UNIQUE_ID_7                 0xF7    // Unique ID byte 7

// Error and Status
#define CMD_ERROR_RESPONSE              'E'     // 0x45 - Error response marker
#define CMD_ECHO                        '!'     // 0x21 - Echo command (ASCII mode)

// Configuration
#define CMD_SET_I2C_ADDRESS             0xC9    // Set I2C address
#define CMD_GET_FLASH_WRITE_COUNT       0xCA    // Get flash write counter
#define CMD_SLEEP                       0xCB    // Enter sleep mode
#define CMD_CONFIGURE_UART              0xCC    // Configure UART parameters

// ============================================================================
// PIN OPERATION COMMANDS
// ============================================================================

// Digital I/O
#define CMD_SET_PIN_HIGH                0xD0    // Set pin high
#define CMD_SET_PIN_LOW                 0xD1    // Set pin low
#define CMD_TOGGLE_PIN                  0xD2    // Toggle pin state
#define CMD_READ_PIN                    0xD3    // Read pin state

// Analog Operations
#define CMD_READ_ADC                    0xA0    // Read ADC value
#define CMD_SET_DAC                     0xA1    // Set DAC value (if available)

// PWM Operations
#define CMD_SET_PWM                     0xB0    // Set PWM duty cycle
#define CMD_SET_PWM_FREQUENCY           0xB1    // Set PWM frequency

// ============================================================================
// SPECIAL COMMANDS
// ============================================================================

// Queue and Data Management
#define CMD_QUEUE_ADD                   0xE0    // Add to queue
#define CMD_QUEUE_READ                  0xE1    // Read from queue
#define CMD_QUEUE_CLEAR                 0xE2    // Clear queue

// Frame Management
#define CMD_FRAME_ADD                   0xE5    // Add frame
#define CMD_FRAME_PROCESS               0xE6    // Process frame

// ASCII Protocol Commands (Phase 2+ only)
#define CMD_ASCII_ECHO                  '!'     // 0x21
#define CMD_ASCII_SET_PUBLIC_DATA       'd'     // 0x64
#define CMD_ASCII_GET_PUBLIC_DATA       'G'     // 0x47
#define CMD_ASCII_LINEFEED              '^'     // 0x5E

// ============================================================================
// COMMAND RANGES (for dispatcher)
// ============================================================================

#define CMD_RANGE_PIN_MODE_START        0x00
#define CMD_RANGE_PIN_MODE_END          0x7F
#define CMD_RANGE_READ_DATA_START       0x81
#define CMD_RANGE_READ_DATA_END         0x8F
#define CMD_RANGE_WRITE_DATA_START      0x91
#define CMD_RANGE_WRITE_DATA_END        0x9F
#define CMD_RANGE_SYSTEM_START          0xF0
#define CMD_RANGE_SYSTEM_END            0xFF

// ============================================================================
// ERROR CODES
// ============================================================================

typedef enum {
    SW_ERROR_NONE = 0,
    SW_ERROR_INVALID_COMMAND = 1,
    SW_ERROR_INVALID_PIN = 2,
    SW_ERROR_INVALID_MODE = 3,
    SW_ERROR_INVALID_PARAMETER = 4,
    SW_ERROR_PIN_NOT_CAPABLE = 5,
    SW_ERROR_UART_ERROR = 10,
    SW_ERROR_I2C_ERROR = 11,
    SW_ERROR_TIMEOUT = 20,
    SW_ERROR_BUFFER_OVERFLOW = 21,
    SW_ERROR_NOT_INITIALIZED = 30,
    SW_ERROR_ALREADY_INITIALIZED = 31,
    SW_ERROR_HARDWARE_ERROR = 40,
    SW_ERROR_UNKNOWN = 255
} SW_ERROR_t;

// ============================================================================
// HELPER MACROS
// ============================================================================

// Extract pin number from pin mode command
#define GET_PIN_FROM_MODE_CMD(cmd)      ((cmd) & 0x0F)

// Extract mode type from pin mode command  
#define GET_MODE_FROM_CMD(cmd)          (((cmd) >> 4) & 0x0F)

// Check if command is in pin mode range
#define IS_PIN_MODE_CMD(cmd)            ((cmd) >= CMD_RANGE_PIN_MODE_START && \
                                         (cmd) <= CMD_RANGE_PIN_MODE_END)

// Check if command is public data read
#define IS_READ_DATA_CMD(cmd)           ((cmd) >= CMD_RANGE_READ_DATA_START && \
                                         (cmd) <= CMD_RANGE_READ_DATA_END)

// Check if command is public data write
#define IS_WRITE_DATA_CMD(cmd)          ((cmd) >= CMD_RANGE_WRITE_DATA_START && \
                                         (cmd) <= CMD_RANGE_WRITE_DATA_END)

// Check if command is system command
#define IS_SYSTEM_CMD(cmd)              ((cmd) >= CMD_RANGE_SYSTEM_START && \
                                         (cmd) <= CMD_RANGE_SYSTEM_END)

#ifdef __cplusplus
}
#endif

#endif // COMMANDS_H
