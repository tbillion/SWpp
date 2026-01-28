/**
 * @file esp32_i2c.c
 * @brief ESP32-S3 I2C Hardware Abstraction Layer Implementation
 * 
 * Implements I2C slave mode for Serial Wombat protocol.
 * Tier 1 (hardware I2C) implementation using ESP-IDF driver.
 * 
 * References:
 * - I2C_IMPLEMENTATION.md Section 5: Tier 1 Hardware I2C
 * - Contract Section 5.1: Hardware First Rule
 */

#include "esp32_i2c.h"
#include "esp32_gpio.h"
#include <string.h>
#include <driver/i2c.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

static const char* TAG = "ESP32_I2C";

// I2C Configuration
#define I2C_NUM                 I2C_NUM_0
#define I2C_SLAVE_RX_BUF_LEN    ESP32_I2C_RX_BUFFER_SIZE
#define I2C_SLAVE_TX_BUF_LEN    ESP32_I2C_TX_BUFFER_SIZE
#define I2C_TASK_PRIORITY       15
#define I2C_TASK_STACK_SIZE     4096

// State
static bool i2c_initialized = false;
static uint8_t i2c_slave_address = ESP32_I2C_BASE_ADDRESS;
static ESP32_I2C_Tier_t current_tier = ESP32_I2C_TIER_1_HARDWARE;
static bool auto_fallback_enabled = true;
static ESP32_I2C_PacketCallback_t packet_callback = NULL;

// Statistics
static ESP32_I2C_Stats_t i2c_stats = {0};

// Packet buffers
static uint8_t rx_packet_buffer[ESP32_I2C_PACKET_SIZE];
static uint8_t tx_packet_buffer[ESP32_I2C_PACKET_SIZE];
static bool rx_packet_ready = false;
static bool tx_packet_ready = false;

// Task handle
static TaskHandle_t i2c_task_handle = NULL;

// Forward declarations
static void i2c_slave_task(void* arg);
static uint8_t read_address_pins(void);
static void configure_address_pins(void);

/**
 * @brief Initialize I2C subsystem
 */
bool ESP32_I2C_Init(void) {
    if (i2c_initialized) {
        ESP_LOGW(TAG, "I2C already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing I2C slave (Tier 1 - Hardware)");

    // Configure address select pins as inputs with pull-ups
    configure_address_pins();

    // Read slave address from pins
    uint8_t addr_offset = read_address_pins();
    i2c_slave_address = ESP32_I2C_BASE_ADDRESS + addr_offset;
    ESP_LOGI(TAG, "I2C address pins: 0x%X, slave address: 0x%02X", addr_offset, i2c_slave_address);

    // Configure I2C slave
    i2c_config_t conf = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = ESP32_I2C_SDA_PIN,
        .scl_io_num = ESP32_I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave = {
            .addr_10bit_en = 0,
            .slave_addr = i2c_slave_address,
            .maximum_speed = ESP32_I2C_CLOCK_SPEED,
        },
        .clk_flags = 0,
    };

    // Configure I2C parameters
    esp_err_t err = i2c_param_config(I2C_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return false;
    }

    // Install I2C driver
    err = i2c_driver_install(I2C_NUM, I2C_MODE_SLAVE, 
                             I2C_SLAVE_RX_BUF_LEN, 
                             I2C_SLAVE_TX_BUF_LEN, 
                             0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    // Create I2C slave task
    BaseType_t task_created = xTaskCreate(
        i2c_slave_task,
        "i2c_slave",
        I2C_TASK_STACK_SIZE,
        NULL,
        I2C_TASK_PRIORITY,
        &i2c_task_handle
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create I2C task");
        i2c_driver_delete(I2C_NUM);
        return false;
    }

    // Reset statistics
    memset(&i2c_stats, 0, sizeof(i2c_stats));
    i2c_stats.current_tier = current_tier;

    // Initialize packet buffers
    memset(rx_packet_buffer, 0, sizeof(rx_packet_buffer));
    memset(tx_packet_buffer, 0, sizeof(tx_packet_buffer));
    rx_packet_ready = false;
    tx_packet_ready = false;

    i2c_initialized = true;
    ESP_LOGI(TAG, "I2C slave initialized: SDA=%d, SCL=%d, Addr=0x%02X", 
             ESP32_I2C_SDA_PIN, ESP32_I2C_SCL_PIN, i2c_slave_address);

    return true;
}

/**
 * @brief Deinitialize I2C subsystem
 */
void ESP32_I2C_Deinit(void) {
    if (!i2c_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing I2C slave");

    // Delete task
    if (i2c_task_handle != NULL) {
        vTaskDelete(i2c_task_handle);
        i2c_task_handle = NULL;
    }

    // Delete driver
    i2c_driver_delete(I2C_NUM);

    i2c_initialized = false;
    ESP_LOGI(TAG, "I2C slave deinitialized");
}

/**
 * @brief Check if I2C is initialized
 */
bool ESP32_I2C_IsInitialized(void) {
    return i2c_initialized;
}

/**
 * @brief Get current I2C slave address
 */
uint8_t ESP32_I2C_GetAddress(void) {
    return i2c_slave_address;
}

/**
 * @brief Read address select pins
 */
uint8_t ESP32_I2C_ReadAddressPins(void) {
    return read_address_pins();
}

/**
 * @brief Update I2C slave address
 */
bool ESP32_I2C_SetAddress(uint8_t address) {
    if (!i2c_initialized) {
        ESP_LOGE(TAG, "I2C not initialized");
        return false;
    }

    if (address < ESP32_I2C_BASE_ADDRESS || address > ESP32_I2C_MAX_ADDRESS) {
        ESP_LOGE(TAG, "Invalid address: 0x%02X (must be 0x%02X-0x%02X)", 
                 address, ESP32_I2C_BASE_ADDRESS, ESP32_I2C_MAX_ADDRESS);
        return false;
    }

    // Note: ESP-IDF doesn't provide i2c_set_slave_address()
    // To change address, need to reinitialize the driver
    ESP_LOGW(TAG, "Changing I2C address requires reinitialization");
    
    i2c_slave_address = address;
    
    // Reinitialize with new address
    ESP32_I2C_Deinit();
    return ESP32_I2C_Init();
}

/**
 * @brief Set packet callback
 */
void ESP32_I2C_SetPacketCallback(ESP32_I2C_PacketCallback_t callback) {
    packet_callback = callback;
    ESP_LOGI(TAG, "Packet callback %s", callback ? "registered" : "cleared");
}

/**
 * @brief Get I2C statistics
 */
void ESP32_I2C_GetStats(ESP32_I2C_Stats_t* stats) {
    if (stats) {
        memcpy(stats, &i2c_stats, sizeof(ESP32_I2C_Stats_t));
    }
}

/**
 * @brief Reset I2C statistics
 */
void ESP32_I2C_ResetStats(void) {
    ESP32_I2C_Tier_t saved_tier = i2c_stats.current_tier;
    memset(&i2c_stats, 0, sizeof(i2c_stats));
    i2c_stats.current_tier = saved_tier;
    ESP_LOGI(TAG, "Statistics reset");
}

/**
 * @brief Get current implementation tier
 */
ESP32_I2C_Tier_t ESP32_I2C_GetTier(void) {
    return current_tier;
}

/**
 * @brief Switch to different implementation tier
 */
bool ESP32_I2C_SetTier(ESP32_I2C_Tier_t tier) {
    if (tier < ESP32_I2C_TIER_1_HARDWARE || tier > ESP32_I2C_TIER_3_SOFTWARE) {
        ESP_LOGE(TAG, "Invalid tier: %d", tier);
        return false;
    }

    if (tier == current_tier) {
        ESP_LOGW(TAG, "Already on tier %d", tier);
        return true;
    }

    ESP_LOGW(TAG, "Tier switching not yet implemented (Tier 2 and 3 TBD)");
    ESP_LOGW(TAG, "Currently only Tier 1 (hardware I2C) is available");
    
    // TODO: Implement tier switching when Tier 2 and 3 are ready
    // For now, only Tier 1 is implemented
    
    return false;
}

/**
 * @brief Enable/disable automatic tier fallback
 */
void ESP32_I2C_SetAutoFallback(bool enable) {
    auto_fallback_enabled = enable;
    ESP_LOGI(TAG, "Auto fallback %s", enable ? "enabled" : "disabled");
}

/**
 * @brief Check if automatic fallback is enabled
 */
bool ESP32_I2C_IsAutoFallbackEnabled(void) {
    return auto_fallback_enabled;
}

/**
 * @brief Manual packet write
 */
bool ESP32_I2C_WritePacket(const uint8_t* packet) {
    if (!i2c_initialized || !packet) {
        return false;
    }

    memcpy(tx_packet_buffer, packet, ESP32_I2C_PACKET_SIZE);
    tx_packet_ready = true;

    // Write to I2C TX buffer
    int ret = i2c_slave_write_buffer(I2C_NUM, tx_packet_buffer, 
                                      ESP32_I2C_PACKET_SIZE, 
                                      pdMS_TO_TICKS(ESP32_I2C_TIMEOUT_MS));
    
    if (ret == ESP32_I2C_PACKET_SIZE) {
        i2c_stats.packets_transmitted++;
        return true;
    } else {
        if (ret < 0) {
            i2c_stats.timeouts++;
        }
        return false;
    }
}

/**
 * @brief Manual packet read
 */
bool ESP32_I2C_ReadPacket(uint8_t* packet) {
    if (!i2c_initialized || !packet) {
        return false;
    }

    if (!rx_packet_ready) {
        return false;
    }

    memcpy(packet, rx_packet_buffer, ESP32_I2C_PACKET_SIZE);
    rx_packet_ready = false;
    
    return true;
}

/**
 * @brief Check if packet is available
 */
bool ESP32_I2C_HasPacket(void) {
    return rx_packet_ready;
}

/**
 * @brief Get number of packets in RX buffer
 */
size_t ESP32_I2C_GetPacketCount(void) {
    // Simplified: we buffer one packet at a time
    return rx_packet_ready ? 1 : 0;
}

/**
 * @brief Configure address select pins
 */
static void configure_address_pins(void) {
    // Configure GPIO 11-14 as inputs with pull-ups
    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL << ESP32_I2C_ADDR_A0_PIN) |
                         (1ULL << ESP32_I2C_ADDR_A1_PIN) |
                         (1ULL << ESP32_I2C_ADDR_A2_PIN) |
                         (1ULL << ESP32_I2C_ADDR_A3_PIN)),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

/**
 * @brief Read address pins and calculate offset
 */
static uint8_t read_address_pins(void) {
    uint8_t addr_offset = 0;
    
    addr_offset |= (gpio_get_level(ESP32_I2C_ADDR_A0_PIN) << 0); // A0
    addr_offset |= (gpio_get_level(ESP32_I2C_ADDR_A1_PIN) << 1); // A1
    addr_offset |= (gpio_get_level(ESP32_I2C_ADDR_A2_PIN) << 2); // A2
    addr_offset |= (gpio_get_level(ESP32_I2C_ADDR_A3_PIN) << 3); // A3
    
    return addr_offset & 0x0F; // Ensure 0-15 range
}

/**
 * @brief I2C slave task
 * 
 * Handles RX/TX of Serial Wombat packets (8 bytes).
 */
static void i2c_slave_task(void* arg) {
    uint8_t rx_data[ESP32_I2C_PACKET_SIZE];
    uint8_t tx_data[ESP32_I2C_PACKET_SIZE];

    ESP_LOGI(TAG, "I2C slave task started");

    while (1) {
        // Wait for incoming data (blocking with timeout)
        int len = i2c_slave_read_buffer(I2C_NUM, rx_data, 
                                        ESP32_I2C_PACKET_SIZE, 
                                        pdMS_TO_TICKS(ESP32_I2C_TIMEOUT_MS));

        if (len == ESP32_I2C_PACKET_SIZE) {
            // Valid 8-byte packet received
            i2c_stats.packets_received++;

            // Copy to RX buffer
            memcpy(rx_packet_buffer, rx_data, ESP32_I2C_PACKET_SIZE);
            rx_packet_ready = true;

            // Process packet
            bool response_ready = false;
            if (packet_callback) {
                // Call callback to process and prepare response
                response_ready = packet_callback(rx_packet_buffer, tx_data);
            } else {
                // No callback, echo the packet (for testing)
                memcpy(tx_data, rx_data, ESP32_I2C_PACKET_SIZE);
                response_ready = true;
            }

            if (response_ready) {
                // Write response to TX buffer for host to read
                int written = i2c_slave_write_buffer(I2C_NUM, tx_data, 
                                                      ESP32_I2C_PACKET_SIZE, 
                                                      pdMS_TO_TICKS(10));
                
                if (written == ESP32_I2C_PACKET_SIZE) {
                    i2c_stats.packets_transmitted++;
                    memcpy(tx_packet_buffer, tx_data, ESP32_I2C_PACKET_SIZE);
                    tx_packet_ready = true;
                } else {
                    ESP_LOGW(TAG, "Failed to write TX packet: %d", written);
                    if (written < 0) {
                        i2c_stats.timeouts++;
                    }
                }
            }

            rx_packet_ready = false; // Processed

        } else if (len > 0) {
            // Incomplete packet
            i2c_stats.incomplete_packets++;
            ESP_LOGW(TAG, "Incomplete I2C packet: %d bytes (expected %d)", 
                     len, ESP32_I2C_PACKET_SIZE);
        } else if (len < 0) {
            // Error or timeout (timeout is normal, ignore)
            // Only log actual errors
            if (len != ESP_ERR_TIMEOUT) {
                i2c_stats.bus_errors++;
                ESP_LOGW(TAG, "I2C read error: %d", len);
            }
        }

        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
