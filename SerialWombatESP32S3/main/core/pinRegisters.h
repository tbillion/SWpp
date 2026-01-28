/**
 * @file pinRegisters.h
 * @brief Pin state management and public data storage for Serial Wombat ESP32-S3
 * 
 * This module manages the state of all pins including their mode configuration,
 * public data buffers, and provides the storage backend for the protocol layer.
 * 
 * Each pin has:
 * - Current mode (0-255)
 * - 7-byte configuration (from mode set command)
 * - 256-byte public data buffer (user-accessible)
 * - Statistics tracking
 * 
 * @author Serial Wombat ESP32-S3 Port
 * @date 2026-01-28
 */

#ifndef PIN_REGISTERS_H
#define PIN_REGISTERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// CONSTANTS
// =============================================================================

#define SW_PIN_COUNT 22                  ///< Total number of pins (18 legacy + 4 extended)
#define SW_PUBLIC_DATA_SIZE 256          ///< Public data buffer size per pin (bytes)
#define SW_PIN_CONFIG_SIZE 7             ///< Configuration size per pin (bytes)

// Pin mode definitions (match SW18AB)
#define SW_PIN_MODE_DIGITAL_INPUT    0x00
#define SW_PIN_MODE_DIGITAL_OUTPUT   0x01
#define SW_PIN_MODE_ANALOG_INPUT     0x02
#define SW_PIN_MODE_PWM_OUTPUT       0x03
// ... more modes defined in SW18AB

// =============================================================================
// ERROR CODES
// =============================================================================

typedef enum {
    SW_OK = 0,
    SW_ERROR_INVALID_PIN,
    SW_ERROR_INVALID_MODE,
    SW_ERROR_INVALID_INDEX,
    SW_ERROR_INVALID_LENGTH,
    SW_ERROR_NOT_INITIALIZED
} SW_Error_t;

// =============================================================================
// PIN STATE STRUCTURE
// =============================================================================

/**
 * @brief Pin state structure
 * 
 * Stores all state information for a single pin including its current mode,
 * configuration, public data buffer, and statistics.
 */
typedef struct {
    uint8_t mode;                           ///< Current pin mode (0-255)
    uint8_t config[SW_PIN_CONFIG_SIZE];     ///< Mode configuration (7 bytes from packet)
    uint8_t publicData[SW_PUBLIC_DATA_SIZE]; ///< User-accessible data buffer
    uint16_t dataIndex;                     ///< Current data read/write index
    
    // Statistics
    uint32_t modeChanges;                   ///< Number of mode changes
    uint32_t dataWrites;                    ///< Number of data writes
    uint32_t dataReads;                     ///< Number of data reads
} SW_PinState_t;

/**
 * @brief Pin registers statistics
 */
typedef struct {
    uint32_t totalModeChanges;              ///< Total mode changes across all pins
    uint32_t totalDataWrites;               ///< Total data writes across all pins
    uint32_t totalDataReads;                ///< Total data reads across all pins
    uint32_t invalidPinAccess;              ///< Invalid pin number access attempts
    uint32_t invalidModeSet;                ///< Invalid mode set attempts
    uint32_t invalidDataAccess;             ///< Invalid data access attempts
} SW_PinRegisters_Stats_t;

// =============================================================================
// API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize pin registers system
 * 
 * Initializes all pin states to default values (mode 0, cleared data).
 * Must be called before using any other pin register functions.
 * 
 * @return SW_OK on success
 */
SW_Error_t SW_PinRegisters_Init(void);

/**
 * @brief Deinitialize pin registers system
 * 
 * Cleans up resources and resets all pin states.
 * 
 * @return SW_OK on success
 */
SW_Error_t SW_PinRegisters_Deinit(void);

// =============================================================================
// PIN MODE MANAGEMENT
// =============================================================================

/**
 * @brief Set pin mode with configuration
 * 
 * Sets the operating mode for a pin and stores the configuration bytes.
 * 
 * @param pin Pin number (0-21)
 * @param mode Pin mode (0-255)
 * @param config Configuration bytes (7 bytes), can be NULL
 * @return SW_OK on success, error code otherwise
 */
SW_Error_t SW_PinRegisters_SetMode(uint8_t pin, uint8_t mode, const uint8_t* config);

/**
 * @brief Get current pin mode
 * 
 * @param pin Pin number (0-21)
 * @return Current mode, or 0xFF if invalid pin
 */
uint8_t SW_PinRegisters_GetMode(uint8_t pin);

/**
 * @brief Get pin configuration
 * 
 * Retrieves the 7-byte configuration for the pin's current mode.
 * 
 * @param pin Pin number (0-21)
 * @param config Output buffer for configuration (must be 7 bytes)
 * @return SW_OK on success, error code otherwise
 */
SW_Error_t SW_PinRegisters_GetConfig(uint8_t pin, uint8_t* config);

// =============================================================================
// PUBLIC DATA ACCESS
// =============================================================================

/**
 * @brief Write data to pin's public data buffer
 * 
 * Writes data to the pin's 256-byte public data buffer at the specified index.
 * 
 * @param pin Pin number (0-21)
 * @param index Starting index in public data buffer (0-255)
 * @param data Data to write
 * @param length Number of bytes to write
 * @return SW_OK on success, error code otherwise
 */
SW_Error_t SW_PinRegisters_WritePublicData(uint8_t pin, uint16_t index, 
                                            const uint8_t* data, size_t length);

/**
 * @brief Read data from pin's public data buffer
 * 
 * Reads data from the pin's 256-byte public data buffer at the specified index.
 * 
 * @param pin Pin number (0-21)
 * @param index Starting index in public data buffer (0-255)
 * @param data Output buffer for data
 * @param length Number of bytes to read
 * @return SW_OK on success, error code otherwise
 */
SW_Error_t SW_PinRegisters_ReadPublicData(uint8_t pin, uint16_t index, 
                                           uint8_t* data, size_t length);

/**
 * @brief Get current data index for pin
 * 
 * @param pin Pin number (0-21)
 * @return Current data index, or 0xFFFF if invalid pin
 */
uint16_t SW_PinRegisters_GetDataIndex(uint8_t pin);

/**
 * @brief Set data index for pin
 * 
 * @param pin Pin number (0-21)
 * @param index New data index
 * @return SW_OK on success, error code otherwise
 */
SW_Error_t SW_PinRegisters_SetDataIndex(uint8_t pin, uint16_t index);

// =============================================================================
// PIN STATE ACCESS
// =============================================================================

/**
 * @brief Get direct access to pin state structure
 * 
 * Returns a pointer to the pin's state structure for direct access.
 * Use with caution - prefer the accessor functions when possible.
 * 
 * @param pin Pin number (0-21)
 * @return Pointer to pin state, or NULL if invalid pin
 */
SW_PinState_t* SW_PinRegisters_GetPinState(uint8_t pin);

/**
 * @brief Clear pin state
 * 
 * Resets a pin to default state (mode 0, cleared data and config).
 * 
 * @param pin Pin number (0-21)
 * @return SW_OK on success, error code otherwise
 */
SW_Error_t SW_PinRegisters_ClearPinState(uint8_t pin);

/**
 * @brief Clear all pin states
 * 
 * Resets all pins to default state.
 * 
 * @return SW_OK on success
 */
SW_Error_t SW_PinRegisters_ClearAllPins(void);

// =============================================================================
// VALIDATION
// =============================================================================

/**
 * @brief Check if pin number is valid
 * 
 * @param pin Pin number to check
 * @return true if valid (0-21), false otherwise
 */
bool SW_PinRegisters_IsValidPin(uint8_t pin);

/**
 * @brief Check if mode is valid for pin
 * 
 * In the ESP32-S3 port, all modes 0-255 are considered valid.
 * This function exists for compatibility and future mode restrictions.
 * 
 * @param pin Pin number (0-21)
 * @param mode Mode to check (0-255)
 * @return true if valid, false otherwise
 */
bool SW_PinRegisters_IsValidMode(uint8_t pin, uint8_t mode);

// =============================================================================
// STATISTICS
// =============================================================================

/**
 * @brief Get pin registers statistics
 * 
 * @param stats Output structure for statistics
 * @return SW_OK on success, error code otherwise
 */
SW_Error_t SW_PinRegisters_GetStats(SW_PinRegisters_Stats_t* stats);

/**
 * @brief Reset pin registers statistics
 * 
 * Resets all statistics counters to zero.
 * 
 * @return SW_OK on success
 */
SW_Error_t SW_PinRegisters_ResetStats(void);

// =============================================================================
// UTILITIES
// =============================================================================

/**
 * @brief Get total number of pins
 * 
 * @return Number of pins (22)
 */
uint8_t SW_PinRegisters_GetPinCount(void);

/**
 * @brief Get public data buffer size per pin
 * 
 * @return Public data size in bytes (256)
 */
uint16_t SW_PinRegisters_GetPublicDataSize(void);

#ifdef __cplusplus
}
#endif

#endif // PIN_REGISTERS_H
