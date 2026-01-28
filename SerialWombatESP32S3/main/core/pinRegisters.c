/**
 * @file pinRegisters.c
 * @brief Pin state management implementation for Serial Wombat ESP32-S3
 * 
 * @author Serial Wombat ESP32-S3 Port
 * @date 2026-01-28
 */

#include "pinRegisters.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// PRIVATE DATA
// =============================================================================

// Pin state storage for all 22 pins
static SW_PinState_t pinStates[SW_PIN_COUNT];

// System statistics
static SW_PinRegisters_Stats_t stats;

// Initialization flag
static bool initialized = false;

// =============================================================================
// INITIALIZATION
// =============================================================================

SW_Error_t SW_PinRegisters_Init(void) {
    // Clear all pin states
    memset(pinStates, 0, sizeof(pinStates));
    
    // Initialize all pins to default mode (0 = Digital Input)
    for (int i = 0; i < SW_PIN_COUNT; i++) {
        pinStates[i].mode = SW_PIN_MODE_DIGITAL_INPUT;
        pinStates[i].dataIndex = 0;
    }
    
    // Clear statistics
    memset(&stats, 0, sizeof(stats));
    
    initialized = true;
    
    printf("[PinRegisters] Initialized %d pins\n", SW_PIN_COUNT);
    return SW_OK;
}

SW_Error_t SW_PinRegisters_Deinit(void) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    // Clear all states
    memset(pinStates, 0, sizeof(pinStates));
    memset(&stats, 0, sizeof(stats));
    
    initialized = false;
    
    printf("[PinRegisters] Deinitialized\n");
    return SW_OK;
}

// =============================================================================
// PIN MODE MANAGEMENT
// =============================================================================

SW_Error_t SW_PinRegisters_SetMode(uint8_t pin, uint8_t mode, const uint8_t* config) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    if (!SW_PinRegisters_IsValidPin(pin)) {
        stats.invalidPinAccess++;
        return SW_ERROR_INVALID_PIN;
    }
    
    // Set mode
    pinStates[pin].mode = mode;
    
    // Copy configuration if provided
    if (config != NULL) {
        memcpy(pinStates[pin].config, config, SW_PIN_CONFIG_SIZE);
    } else {
        memset(pinStates[pin].config, 0, SW_PIN_CONFIG_SIZE);
    }
    
    // Update statistics
    pinStates[pin].modeChanges++;
    stats.totalModeChanges++;
    
    return SW_OK;
}

uint8_t SW_PinRegisters_GetMode(uint8_t pin) {
    if (!initialized || !SW_PinRegisters_IsValidPin(pin)) {
        return 0xFF;  // Invalid
    }
    
    return pinStates[pin].mode;
}

SW_Error_t SW_PinRegisters_GetConfig(uint8_t pin, uint8_t* config) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    if (!SW_PinRegisters_IsValidPin(pin)) {
        stats.invalidPinAccess++;
        return SW_ERROR_INVALID_PIN;
    }
    
    if (config == NULL) {
        return SW_ERROR_INVALID_LENGTH;
    }
    
    memcpy(config, pinStates[pin].config, SW_PIN_CONFIG_SIZE);
    return SW_OK;
}

// =============================================================================
// PUBLIC DATA ACCESS
// =============================================================================

SW_Error_t SW_PinRegisters_WritePublicData(uint8_t pin, uint16_t index, 
                                            const uint8_t* data, size_t length) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    if (!SW_PinRegisters_IsValidPin(pin)) {
        stats.invalidPinAccess++;
        return SW_ERROR_INVALID_PIN;
    }
    
    if (data == NULL || length == 0) {
        return SW_ERROR_INVALID_LENGTH;
    }
    
    // Check bounds
    if (index >= SW_PUBLIC_DATA_SIZE) {
        stats.invalidDataAccess++;
        return SW_ERROR_INVALID_INDEX;
    }
    
    // Limit length to available space
    size_t availableSpace = SW_PUBLIC_DATA_SIZE - index;
    if (length > availableSpace) {
        length = availableSpace;
    }
    
    // Write data
    memcpy(&pinStates[pin].publicData[index], data, length);
    
    // Update statistics
    pinStates[pin].dataWrites++;
    stats.totalDataWrites++;
    
    return SW_OK;
}

SW_Error_t SW_PinRegisters_ReadPublicData(uint8_t pin, uint16_t index, 
                                           uint8_t* data, size_t length) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    if (!SW_PinRegisters_IsValidPin(pin)) {
        stats.invalidPinAccess++;
        return SW_ERROR_INVALID_PIN;
    }
    
    if (data == NULL || length == 0) {
        return SW_ERROR_INVALID_LENGTH;
    }
    
    // Check bounds
    if (index >= SW_PUBLIC_DATA_SIZE) {
        stats.invalidDataAccess++;
        return SW_ERROR_INVALID_INDEX;
    }
    
    // Limit length to available data
    size_t availableData = SW_PUBLIC_DATA_SIZE - index;
    if (length > availableData) {
        length = availableData;
    }
    
    // Read data
    memcpy(data, &pinStates[pin].publicData[index], length);
    
    // Update statistics
    pinStates[pin].dataReads++;
    stats.totalDataReads++;
    
    return SW_OK;
}

uint16_t SW_PinRegisters_GetDataIndex(uint8_t pin) {
    if (!initialized || !SW_PinRegisters_IsValidPin(pin)) {
        return 0xFFFF;  // Invalid
    }
    
    return pinStates[pin].dataIndex;
}

SW_Error_t SW_PinRegisters_SetDataIndex(uint8_t pin, uint16_t index) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    if (!SW_PinRegisters_IsValidPin(pin)) {
        stats.invalidPinAccess++;
        return SW_ERROR_INVALID_PIN;
    }
    
    // Allow index up to 255 (0-255 valid for 256-byte buffer)
    if (index > 255) {
        stats.invalidDataAccess++;
        return SW_ERROR_INVALID_INDEX;
    }
    
    pinStates[pin].dataIndex = index;
    return SW_OK;
}

// =============================================================================
// PIN STATE ACCESS
// =============================================================================

SW_PinState_t* SW_PinRegisters_GetPinState(uint8_t pin) {
    if (!initialized || !SW_PinRegisters_IsValidPin(pin)) {
        return NULL;
    }
    
    return &pinStates[pin];
}

SW_Error_t SW_PinRegisters_ClearPinState(uint8_t pin) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    if (!SW_PinRegisters_IsValidPin(pin)) {
        stats.invalidPinAccess++;
        return SW_ERROR_INVALID_PIN;
    }
    
    // Clear pin state but preserve statistics
    uint32_t modeChanges = pinStates[pin].modeChanges;
    uint32_t dataWrites = pinStates[pin].dataWrites;
    uint32_t dataReads = pinStates[pin].dataReads;
    
    memset(&pinStates[pin], 0, sizeof(SW_PinState_t));
    
    // Restore statistics
    pinStates[pin].modeChanges = modeChanges;
    pinStates[pin].dataWrites = dataWrites;
    pinStates[pin].dataReads = dataReads;
    
    // Set to default mode
    pinStates[pin].mode = SW_PIN_MODE_DIGITAL_INPUT;
    
    return SW_OK;
}

SW_Error_t SW_PinRegisters_ClearAllPins(void) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    for (uint8_t pin = 0; pin < SW_PIN_COUNT; pin++) {
        SW_PinRegisters_ClearPinState(pin);
    }
    
    printf("[PinRegisters] Cleared all %d pins\n", SW_PIN_COUNT);
    return SW_OK;
}

// =============================================================================
// VALIDATION
// =============================================================================

bool SW_PinRegisters_IsValidPin(uint8_t pin) {
    return (pin < SW_PIN_COUNT);
}

bool SW_PinRegisters_IsValidMode(uint8_t pin, uint8_t mode) {
    // For ESP32-S3 port, all modes 0-255 are valid
    // This function exists for future mode restrictions per pin
    (void)pin;    // Unused for now
    (void)mode;   // All modes valid
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

SW_Error_t SW_PinRegisters_GetStats(SW_PinRegisters_Stats_t* statsOut) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    if (statsOut == NULL) {
        return SW_ERROR_INVALID_LENGTH;
    }
    
    memcpy(statsOut, &stats, sizeof(SW_PinRegisters_Stats_t));
    return SW_OK;
}

SW_Error_t SW_PinRegisters_ResetStats(void) {
    if (!initialized) {
        return SW_ERROR_NOT_INITIALIZED;
    }
    
    memset(&stats, 0, sizeof(SW_PinRegisters_Stats_t));
    
    // Also reset per-pin statistics
    for (uint8_t pin = 0; pin < SW_PIN_COUNT; pin++) {
        pinStates[pin].modeChanges = 0;
        pinStates[pin].dataWrites = 0;
        pinStates[pin].dataReads = 0;
    }
    
    printf("[PinRegisters] Statistics reset\n");
    return SW_OK;
}

// =============================================================================
// UTILITIES
// =============================================================================

uint8_t SW_PinRegisters_GetPinCount(void) {
    return SW_PIN_COUNT;
}

uint16_t SW_PinRegisters_GetPublicDataSize(void) {
    return SW_PUBLIC_DATA_SIZE;
}
