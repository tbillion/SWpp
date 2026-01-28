# ESP32-S3 Serial Wombat Timing Model

**Target**: ESP32-S3-N16R8 Port  
**Reference**: Serial Wombat 18AB Timing Behavior  
**Status**: Specification Document  
**Governed By**: PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md  
**Last Updated**: 2026-01-28

---

## 1. EXECUTIVE SUMMARY

This document specifies the timing architecture for the ESP32-S3 Serial Wombat port, ensuring **behavioral equivalence** with SW18AB while adapting to FreeRTOS constraints.

**Key Requirement** (Contract Section 6.1): "The ESP32 port must not change the externally observable timing behavior of SW18AB pin modes."

---

## 2. SW18AB REFERENCE TIMING

### 2.1 Original Architecture (PIC24FJ256GA702)

```
┌─────────────────────────────────────────────────────────────┐
│  PIC24 Bare-Metal Architecture                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  TMR2 (1ms) ──┬──> ISR sets RunForeground = true           │
│               │                                              │
│  Main Loop:   │                                              │
│    while(1) { │                                              │
│      ProcessRx();  ◄── Background (UART/I2C packets)       │
│      if (RunForeground) {                                   │
│        RunForeground = false;                               │
│        ProcessPins();  ◄── Foreground (pin state machines) │
│      }                                                       │
│    }                                                         │
└─────────────────────────────────────────────────────────────┘
```

**Key Timing Characteristics:**
- **Foreground Period**: 1.000ms ±50µs (interrupt latency + variance)
- **Foreground Execution**: Typically 200-800µs depending on active pin modes
- **Overflow Detection**: If ProcessPins() takes >1ms, next cycle delayed
- **DMA Sampling**: 57.6kHz (17.36µs period) via hardware timer
- **Determinism**: High (no OS scheduler, minimal interrupt sources)

---

## 3. ESP32-S3 TARGET TIMING

### 3.1 Timing Requirements (From Contract)

| Requirement | Target | Acceptable Range | Verification |
|-------------|--------|------------------|--------------|
| **Foreground Period** | 1.000ms | ±200µs | Oscilloscope |
| **Jitter** | <100µs | <300µs | Statistical analysis |
| **Overflow Detection** | 100% | 100% | Functional test |
| **DMA Equivalent** | 57.6kHz | ±1% | Timer measurement |
| **Observable Behavior** | Match SW18AB | No protocol violations | Arduino library tests |

---

## 4. ESP32 FREERTOS ARCHITECTURE

### 4.1 Proposed Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  ESP32-S3 FreeRTOS Architecture                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Hardware Timer (TIMG0) ──> ISR (IRAM_ATTR)                    │
│         1ms, high priority     │                                │
│                                ├──> xSemaphoreGiveFromISR()     │
│                                                                  │
│  FreeRTOS Task: foreground_task (Core 0, Priority 24)          │
│    while(1) {                                                   │
│      xSemaphoreTake(sem, portMAX_DELAY);                       │
│      ProcessPins();  ◄── Foreground execution                  │
│    }                                                             │
│                                                                  │
│  FreeRTOS Task: rx_task (Core 1, Priority 20)                  │
│    while(1) {                                                   │
│      ProcessRx();  ◄── UART/I2C packet processing             │
│    }                                                             │
│                                                                  │
│  FreeRTOS Task: supervisor_task (Core 0, Priority 25)          │
│    while(1) {                                                   │
│      vTaskDelay(pdMS_TO_TICKS(100));                           │
│      CheckOverflows();  ◄── Health monitoring                  │
│      CheckStarvation();                                         │
│    }                                                             │
│                                                                  │
│  Hardware Timer (TIMG1) ──> DMA ISR (IRAM_ATTR)                │
│         57.6kHz sampling       │                                │
│                                ├──> Update circular buffers     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. TIMING COMPONENTS

### 5.1 Hardware Timer 0: 1ms Foreground Trigger

**Configuration:**
```c
// ESP-IDF Timer Group 0, Timer 0
timer_config_t config = {
    .divider = 80,              // 80MHz / 80 = 1MHz (1µs tick)
    .counter_dir = TIMER_COUNT_UP,
    .counter_en = TIMER_PAUSE,
    .alarm_en = TIMER_ALARM_EN,
    .auto_reload = TIMER_AUTORELOAD_EN,
};
timer_init(TIMER_GROUP_0, TIMER_0, &config);
timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000); // 1000µs = 1ms
```

**ISR Execution:**
```c
void IRAM_ATTR timer_1ms_isr(void* arg) {
    // Clear interrupt
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);
    
    // Signal foreground task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(foreground_semaphore, &xHigherPriorityTaskWoken);
    
    // Timing diagnostics
    cycles_count++;
    
    // Yield if necessary
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```

**Expected Jitter Sources:**
1. FreeRTOS scheduler (task switch time): ~10-50µs
2. Cache miss (PSRAM access): ~10-30µs
3. Interrupt latency: ~5-20µs
4. Other ISR interference: ~0-100µs (minimized by priority)

**Total Expected Jitter**: ±100-200µs (vs ±50µs on PIC24)

---

### 5.2 Hardware Timer 1: 57.6kHz DMA Sampling

**Configuration:**
```c
// ESP-IDF Timer Group 1, Timer 0
timer_config_t config = {
    .divider = 80,              // 80MHz / 80 = 1MHz
    .counter_dir = TIMER_COUNT_UP,
    .counter_en = TIMER_PAUSE,
    .alarm_en = TIMER_ALARM_EN,
    .auto_reload = TIMER_AUTORELOAD_EN,
};
timer_init(TIMER_GROUP_1, TIMER_0, &config);
timer_set_alarm_value(TIMER_GROUP_1, TIMER_0, 17); // 17.36µs ≈ 57.6kHz
```

**ISR Execution:**
```c
void IRAM_ATTR timer_dma_isr(void* arg) {
    // CRITICAL: Keep ISR execution time <5µs
    
    // Read GPIO input registers (IRAM access, <1µs)
    uint32_t gpio_in = REG_READ(GPIO_IN_REG);
    
    // Write to circular buffer (IRAM, <1µs)
    dma_input_buffer[dma_index] = gpio_in;
    
    // Update index (IRAM, <1µs)
    dma_index = (dma_index + 1) & DMA_BUFFER_MASK;
    
    // Clear interrupt (<1µs)
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_1, TIMER_0);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_1, TIMER_0);
    
    // Total ISR time: ~3-4µs (acceptable at 57.6kHz)
}
```

**CPU Overhead Calculation:**
- ISR frequency: 57,600 Hz
- ISR execution time: ~4µs
- CPU overhead: 57,600 × 4µs = 230,400µs/s = **23% of one core**

**Mitigation:**
- Pin ISR to Core 1 (separate from foreground)
- Use IRAM_ATTR for zero cache miss
- Optional: Hardware DMA (I2S) to reduce to <5% CPU

---

## 6. TASK PRIORITY AND AFFINITY

### 6.1 Priority Scheme

| Task | Core | Priority | Purpose | Rationale |
|------|------|----------|---------|-----------|
| **supervisor_task** | 0 | 25 (highest) | Health monitoring | Must preempt foreground if stuck |
| **foreground_task** | 0 | 24 (high) | ProcessPins() | Critical timing path |
| **rx_task** | 1 | 20 (medium) | Protocol RX | Independent from foreground |
| **dma_isr** | 1 | ISR (highest) | GPIO sampling | Hardware timing critical |
| **timer_1ms_isr** | 0 | ISR (highest) | Foreground trigger | Hardware timing critical |

**Rationale for Core Affinity:**
- **Core 0**: Foreground processing (deterministic, minimal interrupts)
- **Core 1**: Communication and DMA (can tolerate jitter)

---

## 7. OVERFLOW DETECTION AND RECOVERY

### 7.1 Overflow Detection Mechanism

**Definition**: Overflow occurs when `ProcessPins()` takes >1ms, missing the next cycle.

**Detection Method:**
```c
volatile uint32_t cycles_requested = 0;  // ISR increments
volatile uint32_t cycles_completed = 0;  // Task increments

void timer_1ms_isr(void* arg) {
    cycles_requested++;
    xSemaphoreGiveFromISR(...);
}

void foreground_task(void* arg) {
    while (1) {
        xSemaphoreTake(foreground_semaphore, portMAX_DELAY);
        
        uint32_t start_cycles = cycles_completed;
        ProcessPins();
        cycles_completed++;
        
        // Overflow detection
        if (cycles_completed < cycles_requested) {
            overflow_count++;
            ESP_LOGW(TAG, "Frame overflow: %lu behind", 
                     cycles_requested - cycles_completed);
        }
    }
}
```

---

### 7.2 Supervisor Task

**Purpose**: Detect and recover from timing failures.

```c
void supervisor_task(void* arg) {
    uint32_t last_cycles = 0;
    uint32_t stall_count = 0;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms
        
        uint32_t current_cycles = cycles_completed;
        
        // Check for stall (no progress in 100ms)
        if (current_cycles == last_cycles) {
            stall_count++;
            ESP_LOGE(TAG, "Foreground stalled! Cycles: %lu", current_cycles);
            
            if (stall_count > 3) {
                // Critical: restart system
                ESP_LOGE(TAG, "Critical stall, restarting...");
                esp_restart();
            }
        } else {
            stall_count = 0;
        }
        
        // Check overflow rate
        if (overflow_count > 100) {
            ESP_LOGW(TAG, "High overflow rate: %lu in last 100ms", overflow_count);
            // Could reduce feature load or log diagnostics
        }
        
        last_cycles = current_cycles;
        overflow_count = 0; // Reset counter
    }
}
```

---

## 8. TIMING MEASUREMENTS AND VALIDATION

### 8.1 Measurement Points

| Measurement | Method | Acceptance Criteria |
|-------------|--------|---------------------|
| **1ms Jitter** | Oscilloscope on test GPIO | <300µs peak-to-peak |
| **Foreground Duration** | Timer API + logging | <800µs typical, <1000µs max |
| **Overflow Rate** | Counter in supervisor | <0.1% under max load |
| **DMA ISR Duration** | GPIO toggle + scope | <5µs |
| **Protocol Latency** | Packet round-trip | <5ms (unchanged from SW18AB) |

---

### 8.2 Test Procedure

**Test 1: Jitter Measurement**
```c
void test_jitter(void) {
    gpio_set_level(TEST_PIN, 0);
    
    for (int i = 0; i < 10000; i++) {
        gpio_set_level(TEST_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1)); // Wait for semaphore
        gpio_set_level(TEST_PIN, 0);
    }
    
    // Measure TEST_PIN with oscilloscope:
    // - Min period
    // - Max period
    // - Standard deviation
}
```

**Expected Results:**
- Nominal period: 1.000ms
- Min period: 0.850ms (worst-case scheduler delay)
- Max period: 1.200ms (worst-case preemption)
- Standard deviation: <100µs

---

**Test 2: Overflow Stress Test**
```c
void test_overflow(void) {
    // Enable all 40+ pin modes
    // Run for 1 hour
    // Monitor overflow_count
    
    // Acceptance: <0.1% overflow rate
}
```

---

## 9. DEVIATIONS FROM SW18AB

### 9.1 Timing Deviations

| Aspect | SW18AB | ESP32 Port | Impact |
|--------|--------|------------|--------|
| **1ms Jitter** | ±50µs | ±200µs | Minor, within protocol tolerance |
| **Architecture** | Bare-metal | FreeRTOS | Different implementation, same behavior |
| **CPU Overhead** | ~1% (DMA HW) | ~23% (DMA SW) | Acceptable (2-core CPU) |
| **Overflow Detection** | Built-in | Supervisor task | Enhanced monitoring |

**Assessment**: Acceptable. Observable behavior unchanged.

**Documentation**: See DEVIATIONS_FROM_SW18AB.md DEV-TIM-001, DEV-TIM-002

---

## 10. OPTIONAL HIGH-PERFORMANCE MODE

### 10.1 Hardware DMA (I2S Parallel Mode)

**Purpose**: Reduce CPU overhead from 23% to <5%.

**Status**: Phase 2 enhancement (after baseline validation).

**Requirements:**
- Proven stable
- Maintains timing accuracy
- Fully tested
- User-selectable (not default)

**Documentation**: To be added when implemented.

---

## 11. TIMING BUDGET

### 11.1 1ms Cycle Budget

| Activity | Time (µs) | % of Cycle |
|----------|-----------|------------|
| ISR overhead | 10-20 | 1-2% |
| Task switch | 10-50 | 1-5% |
| ProcessPins() | 200-800 | 20-80% |
| Margin | 130-780 | 13-78% |

**Acceptable Load**: ProcessPins() must complete in <900µs worst-case.

---

### 11.2 CPU Budget (Dual-Core)

| Task/ISR | Core | CPU % | Notes |
|----------|------|-------|-------|
| Foreground | 0 | 20-80% | Burst, not continuous |
| Supervisor | 0 | <1% | Periodic check |
| DMA ISR | 1 | 23% | Continuous (or 5% with HW DMA) |
| RX Task | 1 | 5-10% | Burst on packets |
| Idle | Both | 10-50% | Available for future features |

**Total Utilization**: 48-114% (across 2 cores = 200% available)

**Assessment**: Acceptable. Room for future features.

---

## 12. CONTRACT COMPLIANCE

### 12.1 Timing Requirements (Contract Section 6)

| Requirement | Status | Evidence |
|-------------|--------|----------|
| Behavioral parity | ✅ Target | To be measured |
| Execution architecture | ✅ Defined | This document |
| Redundancy / stability | ✅ Designed | Supervisor task |
| Watchdog-safe | ✅ Designed | Bounded ISR, supervisor |

---

## 13. RISK MITIGATION

**Timing Risks** (from RISK_ANALYSIS.md):
- RISK-TIM-001: FreeRTOS jitter → Mitigated by high-priority task, core affinity
- RISK-TIM-002: Missed cycles → Mitigated by supervisor, overflow detection
- RISK-TIM-003: ISR starvation → Mitigated by bounded ISR time, profiling

---

## 14. IMPLEMENTATION CHECKLIST

- [ ] Implement Hardware Timer 0 (1ms trigger)
- [ ] Implement Hardware Timer 1 (57.6kHz DMA)
- [ ] Create foreground_task with semaphore
- [ ] Create rx_task on Core 1
- [ ] Create supervisor_task
- [ ] Implement overflow detection
- [ ] Add timing diagnostics (counters, logs)
- [ ] Profile ISR execution time
- [ ] Measure jitter with oscilloscope
- [ ] Validate with Arduino library
- [ ] Document actual measurements in this file

---

## 15. REVISION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-28 | Initial timing model specification |

---

**Document Status**: SPECIFICATION  
**Implementation Status**: Not started  
**Measurement Status**: Pending implementation

*END OF TIMING MODEL DOCUMENT*
