/**
 * @file esp32_system.h
 * @brief ESP32-S3 System Abstraction Layer
 * 
 * Provides system-level functionality including:
 * - FreeRTOS task management (5 tasks on 2 cores)
 * - Task initialization and lifecycle
 * - Health monitoring and supervisor
 * - System initialization orchestration
 * - Watchdog management
 * 
 * Task Architecture (per TIMING_MODEL.md Section 6):
 * 
 * Core 0 (Protocol Core):
 *   - Foreground Task (P24): 1ms cycle, pin processing
 *   - Supervisor Task (P25): 100ms cycle, health monitoring
 *   - I2C Slave Task (P15): Event-driven
 *   - UART Event Task (P12): Event-driven
 * 
 * Core 1 (Communication Core):
 *   - RX Task (P20): Event-driven, protocol processing
 *   - DMA Timer ISR: 57.6kHz GPIO sampling
 * 
 * @author ESP32 Serial Wombat Port
 * @date 2026-01-28
 */

#ifndef ESP32_SYSTEM_H
#define ESP32_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CONSTANTS
// ============================================================================

/** Task priorities (per TIMING_MODEL.md Section 6.1) */
#define ESP32_SYSTEM_PRIORITY_SUPERVISOR    25  /**< Supervisor task (highest) */
#define ESP32_SYSTEM_PRIORITY_FOREGROUND    24  /**< Foreground task */
#define ESP32_SYSTEM_PRIORITY_RX            20  /**< RX processing task */
#define ESP32_SYSTEM_PRIORITY_I2C           15  /**< I2C slave task */
#define ESP32_SYSTEM_PRIORITY_UART          12  /**< UART event task */

/** Task stack sizes */
#define ESP32_SYSTEM_STACK_FOREGROUND       8192  /**< Foreground task stack */
#define ESP32_SYSTEM_STACK_RX               8192  /**< RX task stack */
#define ESP32_SYSTEM_STACK_SUPERVISOR       4096  /**< Supervisor task stack */
#define ESP32_SYSTEM_STACK_I2C              4096  /**< I2C task stack */
#define ESP32_SYSTEM_STACK_UART             4096  /**< UART task stack */

/** Task core assignments */
#define ESP32_SYSTEM_CORE_PROTOCOL          0  /**< Core 0: Protocol processing */
#define ESP32_SYSTEM_CORE_COMMUNICATION     1  /**< Core 1: Communication */

/** Timing */
#define ESP32_SYSTEM_FOREGROUND_PERIOD_MS   1    /**< Foreground cycle: 1ms */
#define ESP32_SYSTEM_SUPERVISOR_PERIOD_MS   100  /**< Supervisor cycle: 100ms */

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief System health status
 */
typedef struct {
    uint32_t foreground_cycles;      /**< Foreground cycles completed */
    uint32_t foreground_missed;      /**< Missed cycles detected */
    uint32_t foreground_overruns;    /**< Overrun cycles detected */
    uint32_t rx_packets_processed;   /**< RX packets processed */
    uint32_t system_uptime_ms;       /**< System uptime in milliseconds */
    uint32_t free_heap_bytes;        /**< Free heap memory */
    bool supervisor_enabled;         /**< Supervisor is monitoring */
    bool system_healthy;             /**< Overall health status */
} ESP32_System_Health_t;

/**
 * @brief Task statistics
 */
typedef struct {
    const char* name;                /**< Task name */
    eTaskState state;                /**< Task state */
    UBaseType_t priority;            /**< Task priority */
    uint32_t stack_high_water_mark;  /**< Minimum free stack (bytes) */
    BaseType_t core_id;              /**< Core affinity */
} ESP32_System_TaskStats_t;

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize system and all HAL subsystems
 * 
 * Initializes in order:
 * 1. GPIO HAL
 * 2. Timers HAL
 * 3. UART HAL
 * 4. I2C HAL
 * 5. ADC HAL
 * 6. DMA HAL
 * 
 * @return true if all subsystems initialized successfully
 */
bool ESP32_System_Init(void);

/**
 * @brief Deinitialize system and all HAL subsystems
 */
void ESP32_System_Deinit(void);

/**
 * @brief Check if system is initialized
 * 
 * @return true if initialized
 */
bool ESP32_System_IsInitialized(void);

// ============================================================================
// TASK MANAGEMENT
// ============================================================================

/**
 * @brief Start all FreeRTOS tasks
 * 
 * Creates and starts:
 * - Foreground task (Core 0, P24)
 * - Supervisor task (Core 0, P25)
 * - RX task (Core 1, P20)
 * 
 * Note: I2C and UART tasks are started by their respective HALs
 * 
 * @return true if all tasks started successfully
 */
bool ESP32_System_StartTasks(void);

/**
 * @brief Stop all FreeRTOS tasks
 */
void ESP32_System_StopTasks(void);

/**
 * @brief Check if tasks are running
 * 
 * @return true if tasks are running
 */
bool ESP32_System_AreTasksRunning(void);

// ============================================================================
// HEALTH MONITORING
// ============================================================================

/**
 * @brief Get system health status
 * 
 * @param[out] health Pointer to health structure to fill
 */
void ESP32_System_GetHealth(ESP32_System_Health_t* health);

/**
 * @brief Get statistics for all tasks
 * 
 * @param[out] stats Array of task statistics (at least 5 elements)
 * @return Number of tasks filled
 */
uint32_t ESP32_System_GetTaskStats(ESP32_System_TaskStats_t* stats);

/**
 * @brief Reset task statistics
 */
void ESP32_System_ResetTaskStats(void);

// ============================================================================
// SUPERVISOR CONTROL
// ============================================================================

/**
 * @brief Enable supervisor task monitoring
 */
void ESP32_System_EnableSupervisor(void);

/**
 * @brief Disable supervisor task monitoring
 */
void ESP32_System_DisableSupervisor(void);

/**
 * @brief Check if supervisor is enabled
 * 
 * @return true if supervisor is monitoring
 */
bool ESP32_System_IsSupervisorEnabled(void);

// ============================================================================
// WATCHDOG
// ============================================================================

/**
 * @brief Feed the task watchdog timer
 * 
 * Should be called periodically by each task
 */
void ESP32_System_FeedWatchdog(void);

/**
 * @brief Enable task watchdog timer
 */
void ESP32_System_EnableWatchdog(void);

/**
 * @brief Disable task watchdog timer
 */
void ESP32_System_DisableWatchdog(void);

// ============================================================================
// UTILITIES
// ============================================================================

/**
 * @brief Get system uptime in milliseconds
 * 
 * @return Uptime in milliseconds
 */
uint32_t ESP32_System_GetUptimeMs(void);

/**
 * @brief Delay using FreeRTOS vTaskDelay
 * 
 * @param ms Milliseconds to delay
 */
void ESP32_System_Delay(uint32_t ms);

/**
 * @brief Get free heap memory
 * 
 * @return Free heap in bytes
 */
uint32_t ESP32_System_GetFreeHeap(void);

#ifdef __cplusplus
}
#endif

#endif // ESP32_SYSTEM_H
