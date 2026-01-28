# ESP32-S3 I2C Slave Implementation Design

**Target**: ESP32-S3-N16R8 Port  
**Reference**: Serial Wombat 18AB I2C Slave  
**Status**: Design Specification  
**Governed By**: PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md  
**Last Updated**: 2026-01-28

---

## 1. EXECUTIVE SUMMARY

This document specifies the I2C slave implementation for the ESP32-S3 Serial Wombat port, ensuring protocol compatibility while addressing known ESP32 I2C slave limitations.

**Key Requirement** (Contract Section 5.1): "I2C must always target hardware I2C controllers first. Software I2C is fallback only, never default."

---

## 2. SW18AB REFERENCE I2C BEHAVIOR

### 2.1 Original Implementation (PIC24FJ256GA702)

**Hardware**: PIC24 I2C2 peripheral in slave mode

**Protocol**:
1. Host sends 8-byte command packet (I2C write)
2. Serial Wombat processes command
3. Host reads 8-byte response packet (I2C read)
4. Clock stretching used if response not ready

**Timing**:
- Clock speed: 100kHz - 400kHz supported
- Clock stretching: Up to several hundred microseconds
- Response latency: <1ms typical

**Features**:
- 7-bit addressing (default 0x6B)
- Address selection via GPIO pins (4 bits = 16 addresses)
- Clock stretching reliable
- No clock stretch timeout issues

---

## 3. ESP32-S3 I2C CAPABILITIES

### 3.1 Hardware I2C Controllers

ESP32-S3 has **2 hardware I2C controllers**:

| Controller | Pins (Default) | Purpose (Port) | Notes |
|------------|----------------|----------------|-------|
| **I2C0** | SDA=8, SCL=9 | SW slave mode | Primary for protocol |
| **I2C1** | User-defined | Future pin modes | I2C controller mode |

**Capabilities**:
- 7-bit and 10-bit addressing
- Standard (100kHz) and Fast (400kHz) modes
- Slave mode support
- Clock stretching support
- DMA support (RX/TX)

---

### 3.2 Known ESP32 I2C Slave Issues

**Issue 1: Clock Stretching Unreliability**
- ESP-IDF versions <5.0 had clock stretching bugs
- Some hosts timeout during long stretches
- Requires careful tuning

**Issue 2: Slave Response Timing**
- Response must be prepared quickly
- Long processing causes issues

**Issue 3: Buffer Management**
- Requires proper buffer handling
- Overflow can corrupt packets

**Mitigation Status**: ESP-IDF 5.x improves reliability significantly.

---

## 4. IMPLEMENTATION STRATEGY

### 4.1 Three-Tier Approach

```
┌─────────────────────────────────────────────────────────┐
│  Tier 1: Hardware I2C (Default)                         │
│    - ESP-IDF I2C slave driver                           │
│    - DMA-accelerated                                    │
│    - Clock stretching enabled                           │
│    - Extensive error handling                           │
├─────────────────────────────────────────────────────────┤
│  Tier 2: Hardware I2C (Optimized)                       │
│    - Custom ISR handling                                │
│    - Reduced clock stretching                           │
│    - Fast response preparation                          │
├─────────────────────────────────────────────────────────┤
│  Tier 3: Software I2C (Fallback)                        │
│    - Bit-bang on SAME pins (GPIO 8, 9)                 │
│    - User never changes wiring                          │
│    - Lower performance, higher reliability              │
└─────────────────────────────────────────────────────────┘
```

**Selection Logic:**
1. Try Tier 1 (default)
2. If reliability issues detected → Tier 2
3. If still issues → Tier 3 (user-selectable or auto-fallback)

---

## 5. TIER 1: HARDWARE I2C (DEFAULT)

### 5.1 Configuration

```c
// I2C slave configuration
i2c_config_t i2c_slave_config = {
    .mode = I2C_MODE_SLAVE,
    .sda_io_num = GPIO_NUM_8,
    .scl_io_num = GPIO_NUM_9,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .slave = {
        .addr_10bit_en = 0,
        .slave_addr = 0x6B,  // Default SW address
        .maximum_speed = 100000,  // 100kHz (safe)
    },
    .clk_flags = 0,
};

// Initialize I2C
i2c_param_config(I2C_NUM_0, &i2c_slave_config);
i2c_driver_install(I2C_NUM_0, I2C_MODE_SLAVE, 
                   I2C_SLAVE_RX_BUF_LEN, 
                   I2C_SLAVE_TX_BUF_LEN, 0);
```

**Buffer Sizes:**
- RX Buffer: 256 bytes (32 packets)
- TX Buffer: 256 bytes (32 packets)

---

### 5.2 Packet Reception (Write from Host)

```c
void i2c_slave_rx_task(void* arg) {
    uint8_t rx_buffer[8];
    
    while (1) {
        // Wait for data from host (blocking)
        int len = i2c_slave_read_buffer(I2C_NUM_0, rx_buffer, 8, 
                                        pdMS_TO_TICKS(100));
        
        if (len == 8) {
            // Valid packet received
            memcpy(Rxbuffer, rx_buffer, 8);
            
            // Process command (from protocol.c)
            ProcessRx();
            
            // Prepare response for read
            i2c_slave_write_buffer(I2C_NUM_0, Txbuffer, 8, 0);
            
        } else if (len > 0 && len != 8) {
            // Incomplete packet (error)
            ESP_LOGW(TAG, "I2C incomplete packet: %d bytes", len);
            error(SW_ERROR_I2C_INCOMPLETE);
        }
    }
}
```

---

### 5.3 Packet Transmission (Read from Host)

Response prepared during `ProcessRx()` and placed in TX buffer.

```c
// In ProcessRx() after command processing:
i2c_slave_write_buffer(I2C_NUM_0, Txbuffer, 8, pdMS_TO_TICKS(1));
```

**Clock Stretching**: Automatically handled by ESP-IDF if response not ready.

---

### 5.4 Address Selection

**Pins**: GPIO 11-14 (A0-A3)

```c
uint8_t read_i2c_address(void) {
    uint8_t addr_offset = 0;
    
    addr_offset |= (gpio_get_level(GPIO_NUM_11) << 0); // A0
    addr_offset |= (gpio_get_level(GPIO_NUM_12) << 1); // A1
    addr_offset |= (gpio_get_level(GPIO_NUM_13) << 2); // A2
    addr_offset |= (gpio_get_level(GPIO_NUM_14) << 3); // A3
    
    return SW_I2C_BASE_ADDRESS + addr_offset;
}

void configure_i2c_address(void) {
    // Read address pins at startup
    uint8_t addr = read_i2c_address();
    
    // Update I2C slave address
    i2c_set_slave_address(I2C_NUM_0, addr, false);
    
    ESP_LOGI(TAG, "I2C slave address: 0x%02X", addr);
}
```

**Address Range**: 0x6B - 0x7A (16 addresses via A0-A3)

---

## 6. TIER 2: OPTIMIZED HARDWARE I2C

### 6.1 Custom ISR Handler

**Purpose**: Reduce response latency, minimize clock stretching.

```c
void IRAM_ATTR i2c_slave_isr(void* arg) {
    // Custom ISR for fast response
    
    uint32_t status = I2C0.int_status.val;
    
    if (status & I2C_TRANS_COMPLETE_INT_ST) {
        // Transaction complete, prepare next response
        i2c_prepare_response();
    }
    
    if (status & I2C_RXFIFO_FULL_INT_ST) {
        // RX FIFO full, read data quickly
        i2c_read_rxfifo();
    }
    
    // Clear interrupts
    I2C0.int_clr.val = status;
}
```

**Benefits**:
- Faster response preparation
- Reduced clock stretching duration
- Better timing control

**Drawback**: More complex, requires extensive testing.

---

## 7. TIER 3: SOFTWARE I2C (FALLBACK)

### 7.1 Implementation Strategy

**Critical**: Uses **SAME physical pins** as hardware I2C (GPIO 8, 9).

```c
// Software I2C on same pins
#define SW_I2C_SDA  GPIO_NUM_8
#define SW_I2C_SCL  GPIO_NUM_9
```

**State Machine**:
```c
typedef enum {
    I2C_STATE_IDLE,
    I2C_STATE_START,
    I2C_STATE_ADDRESS,
    I2C_STATE_ACK_ADDR,
    I2C_STATE_DATA_RX,
    I2C_STATE_DATA_TX,
    I2C_STATE_ACK_DATA,
    I2C_STATE_STOP
} sw_i2c_state_t;
```

**Implementation**:
- GPIO polling for SCL/SDA
- Bit-bang protocol implementation
- Clock stretching via SCL hold low
- ~100kHz maximum speed

**Advantages**:
- Complete control over timing
- No hardware quirks
- Proven reliability

**Disadvantages**:
- Higher CPU usage (~5-10%)
- Lower maximum speed
- More complex code

---

### 7.2 Automatic Fallback

```c
void i2c_check_reliability(void) {
    static uint32_t error_count = 0;
    static uint32_t total_count = 0;
    
    total_count++;
    
    if (last_i2c_error) {
        error_count++;
    }
    
    // Calculate error rate
    float error_rate = (float)error_count / total_count;
    
    if (error_rate > 0.05 && total_count > 100) {
        // >5% error rate after 100 transactions
        ESP_LOGW(TAG, "I2C unreliable (%.1f%%), switching to software I2C", 
                 error_rate * 100);
        
        i2c_driver_delete(I2C_NUM_0);
        sw_i2c_init(SW_I2C_SDA, SW_I2C_SCL);
        
        i2c_mode = I2C_MODE_SOFTWARE;
    }
}
```

---

## 8. TESTING AND VALIDATION

### 8.1 Test Matrix

| Test | Description | Acceptance Criteria |
|------|-------------|---------------------|
| **Basic Communication** | Send/receive packets | 100% success rate |
| **Address Selection** | Test all 16 addresses | All addresses work |
| **Clock Speed** | Test 100kHz, 400kHz | Both speeds stable |
| **Clock Stretching** | Measure stretch duration | <500µs |
| **Error Recovery** | Inject errors, verify recovery | No system hang |
| **Long-term Stability** | 24-hour stress test | <0.1% error rate |
| **Arduino Library** | Test with real library | Full compatibility |

---

### 8.2 Test Procedure: Arduino Library Validation

```cpp
// Arduino test sketch
#include <SerialWombat.h>

SerialWombatChip sw(0x6B); // ESP32 address

void setup() {
    Wire.begin();
    sw.begin(Wire);
    
    // Test packet echo
    uint8_t version[8];
    sw.readVersion(version);
    
    // Test all commands
    testAllCommands();
}

void loop() {
    // Continuous stress test
    for (int i = 0; i < 1000; i++) {
        sw.readPublicData(0); // Read pin 0 data
    }
}
```

**Validation**: Run for 24 hours, monitor error count.

---

## 9. PERFORMANCE TARGETS

### 9.1 Timing Requirements

| Metric | SW18AB | ESP32 Target | Acceptance |
|--------|--------|--------------|------------|
| **Transaction Time** | ~1ms | <2ms | <3ms |
| **Clock Stretch** | <200µs | <500µs | <1ms |
| **Throughput** | ~1000 pkt/s | >500 pkt/s | >300 pkt/s |
| **Error Rate** | <0.01% | <0.1% | <1% |

---

## 10. DEVIATIONS FROM SW18AB

### 10.1 Implementation Differences

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| **Hardware** | PIC24 I2C2 | ESP32 I2C0 |
| **Clock Stretching** | Reliable | Requires tuning |
| **Fallback** | N/A | Software I2C available |
| **Speed** | 100-400kHz | 100kHz recommended |

**Impact**: May require slower I2C speeds for reliability.

**Documentation**: See DEVIATIONS_FROM_SW18AB.md DEV-COM-002

---

## 11. RISK MITIGATION

**I2C Risks** (from RISK_ANALYSIS.md):
- RISK-HW-003: I2C slave unreliability
  - Mitigation: Three-tier approach, testing, fallback
- RISK-COM-002: Clock stretching timeout
  - Mitigation: Optimize response time, document recommended speed

---

## 12. CONTRACT COMPLIANCE

### 12.1 I2C Requirements (Contract Section 5)

| Requirement | Status | Evidence |
|-------------|--------|----------|
| Hardware first | ✅ Tier 1 default | This document Section 5 |
| Pin stability | ✅ Same pins for HW/SW | Section 7.1 |
| Multiple controllers | ✅ I2C0 + I2C1 | Section 3.1 |
| Address select pins | ✅ GPIO 11-14 | Section 5.4 |
| Required functionality | ✅ Slave mode | All sections |

---

## 13. IMPLEMENTATION CHECKLIST

- [ ] Implement Hardware I2C (Tier 1)
  - [ ] Configure I2C0 as slave
  - [ ] Implement RX task
  - [ ] Implement TX buffer management
  - [ ] Add error handling
- [ ] Implement Address Selection
  - [ ] Read GPIO 11-14 at startup
  - [ ] Configure slave address
- [ ] Test with Arduino Library
  - [ ] Basic communication
  - [ ] All protocol commands
  - [ ] Stress testing
- [ ] Measure Performance
  - [ ] Transaction time
  - [ ] Clock stretch duration
  - [ ] Error rate
- [ ] Implement Fallback (if needed)
  - [ ] Software I2C on same pins
  - [ ] Automatic fallback logic
- [ ] Documentation
  - [ ] User guide
  - [ ] Troubleshooting
  - [ ] Performance results

---

## 14. USER CONFIGURATION

### 14.1 Compile-Time Options

```c
// In sdkconfig or project configuration
#define CONFIG_SW_I2C_MODE_AUTO       1  // Auto-select HW/SW
#define CONFIG_SW_I2C_MODE_HARDWARE   2  // Force hardware
#define CONFIG_SW_I2C_MODE_SOFTWARE   3  // Force software

#define CONFIG_SW_I2C_MODE CONFIG_SW_I2C_MODE_AUTO
```

---

### 14.2 Runtime Configuration

```c
// Command to switch I2C mode (for testing)
// Command: 0xF0 (custom diagnostic)
// Data[0]: 0x00 = Auto, 0x01 = HW, 0x02 = SW
```

---

## 15. REVISION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-28 | Initial I2C implementation design |

---

**Document Status**: SPECIFICATION  
**Implementation Status**: Not started  
**Testing Status**: Pending implementation

*END OF I2C IMPLEMENTATION DOCUMENT*
