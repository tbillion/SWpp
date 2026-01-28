/**
 * @file esp32_system.c
 * @brief ESP32-S3 System Abstraction Layer Implementation
 */

#include "esp32_system.h"
#include "esp32_gpio.h"
#include "esp32_timers.h"
#include "esp32_uart.h"
#include "esp32_i2c.h"
#include "esp32_adc.h"
#include "esp32_dma.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"

static const char* TAG = "ESP32_System";

// ============================================================================
// GLOBAL STATE
// ============================================================================

static bool g_system_initialized = false;
static bool g_tasks_running = false;
static bool g_supervisor_enabled = true;

// Task handles
static TaskHandle_t g_foreground_task_handle = NULL;
static TaskHandle_t g_supervisor_task_handle = NULL;
static TaskHandle_t g_rx_task_handle = NULL;

// Semaphore for 1ms timer
static SemaphoreHandle_t g_foreground_semaphore = NULL;

// Statistics
static volatile uint32_t g_foreground_cycles = 0;
static volatile uint32_t g_foreground_missed = 0;
static volatile uint32_t g_foreground_overruns = 0;
static volatile uint32_t g_rx_packets = 0;
static uint32_t g_last_cycle_count = 0;

// ============================================================================
// TASK FUNCTIONS
// ============================================================================

/**
 * @brief Foreground task - 1ms cycle for pin processing
 */
static void foreground_task(void* pvParameters) {
    ESP_LOGI(TAG, "Foreground task started on core %d", xPortGetCoreID());
    
    while (1) {
        // Wait for 1ms timer semaphore
        if (xSemaphoreTake(g_foreground_semaphore, portMAX_DELAY) == pdTRUE) {
            // Increment cycle counter
            g_foreground_cycles++;
            
            // TODO: Process pin modes here (Phase 3)
            // This is where Serial Wombat pin mode processing will occur
            
            // Feed watchdog
            ESP32_System_FeedWatchdog();
        }
    }
}

/**
 * @brief Supervisor task - monitors foreground task health
 */
static void supervisor_task(void* pvParameters) {
    ESP_LOGI(TAG, "Supervisor task started on core %d", xPortGetCoreID());
    
    uint32_t last_check_cycles = 0;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(ESP32_SYSTEM_SUPERVISOR_PERIOD_MS));
        
        if (!g_supervisor_enabled) {
            continue;
        }
        
        // Check foreground task health
        uint32_t current_cycles = g_foreground_cycles;
        uint32_t cycles_since_last = current_cycles - last_check_cycles;
        
        // Expected cycles in 100ms = 100 (at 1kHz)
        uint32_t expected_cycles = ESP32_SYSTEM_SUPERVISOR_PERIOD_MS / ESP32_SYSTEM_FOREGROUND_PERIOD_MS;
        
        if (cycles_since_last == 0) {
            // No cycles - foreground task may be hung
            g_foreground_missed++;
            ESP_LOGW(TAG, "Supervisor: Foreground task missed cycles! Total missed: %lu", g_foreground_missed);
        } else if (cycles_since_last > expected_cycles * 1.1) {
            // Too many cycles - possible overrun
            g_foreground_overruns++;
            ESP_LOGW(TAG, "Supervisor: Foreground task overrun! Got %lu, expected ~%lu", cycles_since_last, expected_cycles);
        }
        
        last_check_cycles = current_cycles;
        
        // Feed watchdog
        ESP32_System_FeedWatchdog();
    }
}

/**
 * @brief RX task - processes received UART/I2C data
 */
static void rx_task(void* pvParameters) {
    ESP_LOGI(TAG, "RX task started on core %d", xPortGetCoreID());
    
    while (1) {
        // TODO: Process UART/I2C received data (Phase 3)
        // This is where Serial Wombat protocol parsing will occur
        
        // For now, just delay to avoid busy loop
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Feed watchdog
        ESP32_System_FeedWatchdog();
    }
}

/**
 * @brief Callback for 1ms timer - signals foreground task
 */
static void timer_1ms_callback(uint64_t timestamp_us) {
    // Signal foreground task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(g_foreground_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool ESP32_System_Init(void) {
    if (g_system_initialized) {
        ESP_LOGW(TAG, "System already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing ESP32 Serial Wombat system...");
    
    // Initialize GPIO HAL
    ESP_LOGI(TAG, "Initializing GPIO HAL...");
    if (!ESP32_GPIO_Init()) {
        ESP_LOGE(TAG, "Failed to initialize GPIO HAL");
        return false;
    }
    
    // Initialize Timers HAL
    ESP_LOGI(TAG, "Initializing Timers HAL...");
    if (!ESP32_Timers_Init()) {
        ESP_LOGE(TAG, "Failed to initialize Timers HAL");
        return false;
    }
    
    // Initialize UART HAL
    ESP_LOGI(TAG, "Initializing UART HAL...");
    if (!ESP32_UART_Init()) {
        ESP_LOGE(TAG, "Failed to initialize UART HAL");
        return false;
    }
    
    // Initialize I2C HAL
    ESP_LOGI(TAG, "Initializing I2C HAL...");
    if (!ESP32_I2C_Init()) {
        ESP_LOGE(TAG, "Failed to initialize I2C HAL");
        return false;
    }
    
    // Initialize ADC HAL
    ESP_LOGI(TAG, "Initializing ADC HAL...");
    ESP32_ADC_Config_t adc_config = {
        .samples_to_average = 4
    };
    if (!ESP32_ADC_Init(&adc_config)) {
        ESP_LOGE(TAG, "Failed to initialize ADC HAL");
        return false;
    }
    
    // Initialize DMA HAL
    ESP_LOGI(TAG, "Initializing DMA HAL...");
    ESP32_DMA_Config_t dma_config = {
        .buffer_size = 1024
    };
    if (!ESP32_DMA_Init(&dma_config)) {
        ESP_LOGE(TAG, "Failed to initialize DMA HAL");
        return false;
    }
    
    // Create semaphore for 1ms timer
    g_foreground_semaphore = xSemaphoreCreateBinary();
    if (g_foreground_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create foreground semaphore");
        return false;
    }
    
    g_system_initialized = true;
    ESP_LOGI(TAG, "System initialization complete!");
    
    return true;
}

void ESP32_System_Deinit(void) {
    if (!g_system_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing system...");
    
    // Stop tasks first
    ESP32_System_StopTasks();
    
    // Deinitialize HAL components
    ESP32_DMA_Deinit();
    ESP32_ADC_Deinit();
    ESP32_I2C_Deinit();
    ESP32_UART_Deinit();
    ESP32_Timers_Deinit();
    // GPIO doesn't have deinit
    
    // Delete semaphore
    if (g_foreground_semaphore != NULL) {
        vSemaphoreDelete(g_foreground_semaphore);
        g_foreground_semaphore = NULL;
    }
    
    g_system_initialized = false;
    ESP_LOGI(TAG, "System deinitialized");
}

bool ESP32_System_IsInitialized(void) {
    return g_system_initialized;
}

// ============================================================================
// TASK MANAGEMENT
// ============================================================================

bool ESP32_System_StartTasks(void) {
    if (!g_system_initialized) {
        ESP_LOGE(TAG, "System not initialized");
        return false;
    }
    
    if (g_tasks_running) {
        ESP_LOGW(TAG, "Tasks already running");
        return true;
    }
    
    ESP_LOGI(TAG, "Starting FreeRTOS tasks...");
    
    // Create foreground task (Core 0, P24)
    BaseType_t ret = xTaskCreatePinnedToCore(
        foreground_task,
        "foreground",
        ESP32_SYSTEM_STACK_FOREGROUND,
        NULL,
        ESP32_SYSTEM_PRIORITY_FOREGROUND,
        &g_foreground_task_handle,
        ESP32_SYSTEM_CORE_PROTOCOL
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create foreground task");
        return false;
    }
    
    // Create supervisor task (Core 0, P25)
    ret = xTaskCreatePinnedToCore(
        supervisor_task,
        "supervisor",
        ESP32_SYSTEM_STACK_SUPERVISOR,
        NULL,
        ESP32_SYSTEM_PRIORITY_SUPERVISOR,
        &g_supervisor_task_handle,
        ESP32_SYSTEM_CORE_PROTOCOL
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create supervisor task");
        return false;
    }
    
    // Create RX task (Core 1, P20)
    ret = xTaskCreatePinnedToCore(
        rx_task,
        "rx_processing",
        ESP32_SYSTEM_STACK_RX,
        NULL,
        ESP32_SYSTEM_PRIORITY_RX,
        &g_rx_task_handle,
        ESP32_SYSTEM_CORE_COMMUNICATION
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        return false;
    }
    
    // Start 1ms timer with callback (signals foreground task)
    if (!ESP32_Timers_Start1ms(timer_1ms_callback, NULL)) {
        ESP_LOGE(TAG, "Failed to start 1ms timer");
        return false;
    }
    
    // Start DMA sampling
    if (!ESP32_DMA_Start()) {
        ESP_LOGE(TAG, "Failed to start DMA sampling");
        return false;
    }
    
    g_tasks_running = true;
    ESP_LOGI(TAG, "All tasks started successfully!");
    
    return true;
}

void ESP32_System_StopTasks(void) {
    if (!g_tasks_running) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping tasks...");
    
    // Stop timers
    ESP32_Timers_Stop1ms();
    ESP32_Timers_StopDMA();
    
    // Stop DMA
    ESP32_DMA_Stop();
    
    // Delete tasks
    if (g_foreground_task_handle != NULL) {
        vTaskDelete(g_foreground_task_handle);
        g_foreground_task_handle = NULL;
    }
    
    if (g_supervisor_task_handle != NULL) {
        vTaskDelete(g_supervisor_task_handle);
        g_supervisor_task_handle = NULL;
    }
    
    if (g_rx_task_handle != NULL) {
        vTaskDelete(g_rx_task_handle);
        g_rx_task_handle = NULL;
    }
    
    g_tasks_running = false;
    ESP_LOGI(TAG, "Tasks stopped");
}

bool ESP32_System_AreTasksRunning(void) {
    return g_tasks_running;
}

// ============================================================================
// HEALTH MONITORING
// ============================================================================

void ESP32_System_GetHealth(ESP32_System_Health_t* health) {
    if (health == NULL) {
        return;
    }
    
    health->foreground_cycles = g_foreground_cycles;
    health->foreground_missed = g_foreground_missed;
    health->foreground_overruns = g_foreground_overruns;
    health->rx_packets_processed = g_rx_packets;
    health->system_uptime_ms = ESP32_System_GetUptimeMs();
    health->free_heap_bytes = ESP32_System_GetFreeHeap();
    health->supervisor_enabled = g_supervisor_enabled;
    health->system_healthy = (g_foreground_missed == 0) && (g_foreground_overruns == 0);
}

uint32_t ESP32_System_GetTaskStats(ESP32_System_TaskStats_t* stats) {
    if (stats == NULL) {
        return 0;
    }
    
    uint32_t count = 0;
    
    // Foreground task
    if (g_foreground_task_handle != NULL) {
        stats[count].name = "Foreground";
        stats[count].state = eTaskGetState(g_foreground_task_handle);
        stats[count].priority = uxTaskPriorityGet(g_foreground_task_handle);
        stats[count].stack_high_water_mark = uxTaskGetStackHighWaterMark(g_foreground_task_handle);
        stats[count].core_id = ESP32_SYSTEM_CORE_PROTOCOL;
        count++;
    }
    
    // Supervisor task
    if (g_supervisor_task_handle != NULL) {
        stats[count].name = "Supervisor";
        stats[count].state = eTaskGetState(g_supervisor_task_handle);
        stats[count].priority = uxTaskPriorityGet(g_supervisor_task_handle);
        stats[count].stack_high_water_mark = uxTaskGetStackHighWaterMark(g_supervisor_task_handle);
        stats[count].core_id = ESP32_SYSTEM_CORE_PROTOCOL;
        count++;
    }
    
    // RX task
    if (g_rx_task_handle != NULL) {
        stats[count].name = "RX";
        stats[count].state = eTaskGetState(g_rx_task_handle);
        stats[count].priority = uxTaskPriorityGet(g_rx_task_handle);
        stats[count].stack_high_water_mark = uxTaskGetStackHighWaterMark(g_rx_task_handle);
        stats[count].core_id = ESP32_SYSTEM_CORE_COMMUNICATION;
        count++;
    }
    
    return count;
}

void ESP32_System_ResetTaskStats(void) {
    g_foreground_cycles = 0;
    g_foreground_missed = 0;
    g_foreground_overruns = 0;
    g_rx_packets = 0;
    g_last_cycle_count = 0;
}

// ============================================================================
// SUPERVISOR CONTROL
// ============================================================================

void ESP32_System_EnableSupervisor(void) {
    g_supervisor_enabled = true;
    ESP_LOGI(TAG, "Supervisor enabled");
}

void ESP32_System_DisableSupervisor(void) {
    g_supervisor_enabled = false;
    ESP_LOGI(TAG, "Supervisor disabled");
}

bool ESP32_System_IsSupervisorEnabled(void) {
    return g_supervisor_enabled;
}

// ============================================================================
// WATCHDOG
// ============================================================================

void ESP32_System_FeedWatchdog(void) {
    // Feed task watchdog if enabled
    esp_task_wdt_reset();
}

void ESP32_System_EnableWatchdog(void) {
    // Watchdog is enabled by default in sdkconfig
    ESP_LOGI(TAG, "Task watchdog enabled");
}

void ESP32_System_DisableWatchdog(void) {
    // Not recommended in production
    ESP_LOGW(TAG, "Task watchdog disabled (not recommended)");
}

// ============================================================================
// UTILITIES
// ============================================================================

uint32_t ESP32_System_GetUptimeMs(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void ESP32_System_Delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint32_t ESP32_System_GetFreeHeap(void) {
    return esp_get_free_heap_size();
}
