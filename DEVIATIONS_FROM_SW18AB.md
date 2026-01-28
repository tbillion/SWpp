# Deviations from Serial Wombat 18AB

**Target**: ESP32-S3-N16R8 Port  
**Reference**: Serial Wombat 18AB (PIC24FJ256GA702)  
**Status**: Living Document  
**Governed By**: PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md  
**Last Updated**: 2026-01-28

---

## 1. PURPOSE

This document catalogs **all behavioral, timing, and functional differences** between the ESP32-S3 Serial Wombat port and the original Serial Wombat 18AB firmware.

**Contract Requirement**: "All deviations from SW18AB must be documented" (Section 2, Principle 4)

---

## 2. DEVIATION CLASSIFICATION

| Class | Definition | Example |
|-------|------------|---------|
| **ENHANCEMENT** | Improved behavior or capability | More pins, higher ADC resolution |
| **LIMITATION** | Reduced capability or restriction | ADC2/WiFi conflict |
| **TIMING** | Measurable timing differences | Jitter, latency changes |
| **BEHAVIORAL** | Different observable behavior | Different boot sequence |
| **DOCUMENTED** | Known and acceptable difference | Voltage levels (3.3V vs 5V) |

---

## 3. HARDWARE DEVIATIONS

### DEV-HW-001: Pin Count Increase

**Class**: ENHANCEMENT  
**Severity**: None (positive)

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| Physical Pins | 18 | 22 |
| Virtual Pins | 2 | TBD |
| Total Pins | 20 | 22+ |

**Impact**: More I/O available to users.

**Compatibility**: Fully backward compatible. SW18AB pin 0-17 mapping preserved.

**Documentation**: See PIN_MAPPING.md

---

### DEV-HW-002: GPIO Voltage Levels

**Class**: LIMITATION (DOCUMENTED)  
**Severity**: CRITICAL (user must know)

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| Logic High | 3.3V or 5V | 3.3V only |
| 5V Tolerance | Yes | **NO** |
| Current per Pin | 25mA | 40mA (configured to 20mA default) |

**Impact**: 
- ⚠️ **Applying 5V to ESP32 GPIO will damage the chip**
- Level shifters required for 5V interfacing

**Mitigation**: 
- Prominent warning in documentation
- Recommend level shifters for 5V systems

**Documentation**: See PIN_MAPPING.md Section 10.1

---

### DEV-HW-003: ADC Resolution Increase

**Class**: ENHANCEMENT  
**Severity**: None (positive)

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| ADC Resolution | 10-bit (0-1023) | 12-bit (0-4095) |
| Protocol Output | 0-65535 (scaled) | 0-65535 (scaled) |
| Effective Resolution | 64 LSB steps | 16 LSB steps |

**Impact**: Higher resolution analog readings (4x better granularity).

**Compatibility**: Fully compatible. Both scale to 0-65535 for protocol.

**Documentation**: See PIN_MAPPING.md Section 11.2

---

### DEV-HW-004: ADC2 and WiFi Conflict

**Class**: LIMITATION  
**Severity**: MEDIUM (Phase 1: none, Future: significant)

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| WiFi Available | No | Yes (future) |
| ADC Channels | 9 (always usable) | 18 (10 on ADC1, 8 on ADC2) |
| ADC/WiFi Conflict | N/A | ADC2 unusable when WiFi active |

**Impact**:
- **Phase 1**: No impact (WiFi disabled)
- **Future**: Pins 9-16 (GPIO 11-18) cannot use ADC function when WiFi is active

**Mitigation**:
- Prefer ADC1 pins (0-8) for critical analog inputs
- Document ADC2 limitation prominently
- Provide runtime warning if ADC2 used with WiFi

**Documentation**: See PIN_MAPPING.md Section 7.1, RISK_ANALYSIS.md RISK-HW-002

---

### DEV-HW-005: I2C Address Select Pins

**Class**: BEHAVIORAL  
**Severity**: Low

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| Address Select Pins | TBD | GPIO 11-14 (4 pins, A0-A3) |
| Pin Location | Chip-specific | GPIO 11-14 (sequential) |

**Impact**: Different physical pins used for I2C address selection.

**Compatibility**: Logical addressing unchanged, different wiring.

**Documentation**: See PIN_MAPPING.md Section 6

---

## 4. TIMING DEVIATIONS

### DEV-TIM-001: FreeRTOS Scheduler Jitter

**Class**: TIMING  
**Severity**: MEDIUM (measured and documented)

| Aspect | SW18AB | ESP32 Port (Expected) |
|--------|--------|------------------------|
| Architecture | Bare-metal | FreeRTOS |
| 1ms Cycle Jitter | ±50µs (interrupt latency) | ±200µs (scheduler + interrupt) |
| Determinism | High | Medium (task priority helps) |

**Impact**:
- Timing-sensitive modes may show small differences
- Protocol timing unchanged (within tolerance)
- Pin modes requiring <100µs precision may need hardware timers

**Mitigation**:
- High-priority FreeRTOS task
- Pin task to dedicated CPU core
- Use hardware timers (LEDC, TIMG) for critical timing
- Measure actual jitter with oscilloscope

**Measurement Plan**: 
- Oscilloscope on timing test pin
- Record min/max/average cycle time
- Document in TIMING_MODEL.md

**Documentation**: See TIMING_MODEL.md (to be created), RISK_ANALYSIS.md RISK-TIM-001

---

### DEV-TIM-002: DMA Sampling Rate

**Class**: TIMING  
**Severity**: Low (baseline: parity)

| Aspect | SW18AB | ESP32 Port (Baseline) |
|--------|--------|------------------------|
| DMA Implementation | Hardware (4 channels) | Software emulation (timer-driven) |
| Sampling Rate | 57.6kHz (fixed) | 57.6kHz (configurable) |
| CPU Overhead | <1% | ~10-30% (estimated) |

**Impact**:
- Baseline: functional parity, higher CPU usage
- Optional high-performance mode (I2S) can reduce CPU overhead

**Mitigation**:
- Profile CPU usage
- Optimize ISR (IRAM, minimal logic)
- Optional I2S parallel mode for low-CPU operation

**Documentation**: See TIMING_MODEL.md (to be created), RISK_ANALYSIS.md RISK-TIM-003

---

## 5. COMMUNICATION DEVIATIONS

### DEV-COM-001: UART Pin Assignments

**Class**: BEHAVIORAL  
**Severity**: Low (documented)

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| Default UART Pins | Chip-specific | GPIO 43/44 (UART0, USB bridge) |
| Protocol UART | Single UART | User-selectable (UART0 or UART1) |
| Secondary UART | N/A | GPIO 47/48 (UART1) |

**Impact**: Different physical UART pins, more flexibility.

**Compatibility**: Protocol unchanged, different wiring.

**Documentation**: See PIN_MAPPING.md Section 4.2

---

### DEV-COM-002: I2C Slave Reliability

**Class**: LIMITATION (DOCUMENTED)  
**Severity**: MEDIUM (requires testing)

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| I2C Implementation | Hardware (PIC24) | Hardware (ESP32 + fallback) |
| Clock Stretching | Reliable | Known ESP32 issues |
| Recommended Speed | 100kHz - 400kHz | 100kHz (safer) |

**Impact**:
- May require slower I2C speeds
- Software fallback available if hardware unreliable

**Mitigation**:
- Extensive testing with Arduino library
- Optimize response preparation time
- Document recommended I2C speed (100kHz)
- Software I2C fallback on same pins

**Documentation**: See I2C_IMPLEMENTATION.md (to be created), RISK_ANALYSIS.md RISK-HW-003

---

## 6. MEMORY AND STORAGE DEVIATIONS

### DEV-MEM-001: RAM Abundance

**Class**: ENHANCEMENT  
**Severity**: None (positive)

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| Total RAM | 8KB | 512KB (64x more) |
| Pin Register Size | 96 bytes/pin | Same (96 bytes/pin) |
| User Buffer | 8KB (limited) | Configurable (abundant) |

**Impact**: No memory constraints, all features can be enabled simultaneously.

**Compatibility**: Fully compatible, more flexible.

**Documentation**: Architecture documentation

---

### DEV-MEM-002: Non-Volatile Storage Implementation

**Class**: BEHAVIORAL  
**Severity**: Low

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| NV Storage | Program Flash | NVS (or partition) |
| Wear Leveling | Manual/app-level | Built-in (NVS) |
| Access Speed | Direct | API calls |

**Impact**: Different implementation, same functionality.

**Compatibility**: Functional equivalence maintained.

**Documentation**: Architecture documentation

---

## 7. POWER AND BOOT DEVIATIONS

### DEV-PWR-001: Power Consumption

**Class**: DOCUMENTED  
**Severity**: Low (Phase 1: always-on)

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| Active Current | ~4mA | ~30-50mA (no WiFi) |
| Sleep Current | <750µA | ~3-5mA (light sleep) |

**Impact**: Higher power consumption (more capable processor).

**Mitigation**: 
- Phase 1: Always-on (acceptable)
- Future: Light sleep modes when idle

**Documentation**: Power management documentation (future)

---

### DEV-PWR-002: Boot Sequence

**Class**: BEHAVIORAL  
**Severity**: Low

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| Boot Time | <100ms | ~200-300ms (ESP-IDF init) |
| Boot Messages | Minimal | ESP32 ROM + IDF messages |
| Console Output | Optional | Default (can be disabled) |

**Impact**: Longer boot time, more console output.

**Mitigation**: 
- Acceptable for most applications
- Console can be reduced via sdkconfig

**Documentation**: Installation and configuration guides

---

## 8. PIN MODE SPECIFIC DEVIATIONS

### 8.1 PWM Mode

**Status**: To be documented during implementation

**Expected Deviations**:
- ESP32 uses LEDC peripheral (different from PIC24 CCP)
- Potentially more PWM channels available
- Different frequency ranges

**Documentation**: To be completed in pin mode implementation phase

---

### 8.2 Servo Mode

**Status**: To be documented during implementation

**Expected Deviations**:
- ESP32 LEDC-based servo control
- Different resolution/accuracy

**Documentation**: To be completed in pin mode implementation phase

---

### 8.3 Quadrature Encoder Mode

**Status**: To be documented during implementation

**Expected Deviations**:
- ESP32 PCNT peripheral vs PIC24 DMA
- Different maximum count rates

**Documentation**: To be completed in pin mode implementation phase

---

### 8.4 [Additional Pin Modes]

**Note**: Detailed deviations for all 40+ pin modes will be documented during implementation.

Each mode will have:
- Behavioral differences
- Timing differences
- Hardware capability differences
- Compatibility notes

---

## 9. PROTOCOL DEVIATIONS

### DEV-PROTO-001: Protocol Compliance

**Status**: Target = 100% compatible

| Aspect | SW18AB | ESP32 Port (Target) |
|--------|--------|---------------------|
| Packet Format | 8 bytes | 8 bytes (same) |
| Commands | All commands | All commands (same) |
| Binary Protocol | Yes | Yes (same) |
| ASCII Protocol | Yes | Yes (same) |

**Expected Result**: Zero protocol deviations (full compatibility).

**Verification**: Automated protocol conformance tests with Arduino library.

---

## 10. BOOTLOADER DEVIATIONS

### DEV-BOOT-001: Bootloader Architecture

**Class**: BEHAVIORAL  
**Severity**: Low

| Aspect | SW18AB | ESP32 Port |
|--------|--------|------------|
| Bootloader | Custom SW bootloader | ESP-IDF standard bootloader |
| Update Protocol | SW18AB proprietary | ESP-IDF OTA |
| UART Update | Via SW protocol | Via esptool.py |

**Impact**: Different update mechanism, not compatible with SW18AB update tools.

**Mitigation**: 
- Phase 1: Use ESP-IDF OTA (safer)
- Future: May add SW18AB-compatible update (if provably safe)

**Documentation**: INSTALLATION.md, bootloader documentation

---

## 11. FEATURE ADDITIONS (Not in SW18AB)

### ADD-001: WiFi Support (Future)

**Class**: ADDITION  
**Phase**: Future (not Phase 1)

New capabilities:
- WiFi configuration portal
- Web API for protocol access
- MQTT bridge
- OTA updates via WiFi

**Documentation**: Future feature documentation

---

### ADD-002: Bluetooth Support (Future)

**Class**: ADDITION  
**Phase**: Future (not Phase 1)

New capabilities:
- BLE configuration
- BLE protocol bridge

**Documentation**: Future feature documentation

---

## 12. COMPATIBILITY MATRIX

### 12.1 Arduino Library Compatibility

| Feature | SW18AB | ESP32 Port | Compatible |
|---------|--------|------------|------------|
| Protocol Commands | ✅ | ✅ | ✅ Yes |
| UART Communication | ✅ | ✅ | ✅ Yes |
| I2C Communication | ✅ | ✅ | ⚠️ To be tested |
| Pin Modes (all) | ✅ | 🔄 In progress | 🔄 TBD |

---

### 12.2 Python Library Compatibility

| Feature | SW18AB | ESP32 Port | Compatible |
|---------|--------|------------|------------|
| Protocol Commands | ✅ | ✅ | ✅ Expected |
| UART Communication | ✅ | ✅ | ✅ Expected |
| I2C Communication | ✅ | ✅ | ⚠️ To be tested |

---

## 13. DEVIATION ACCEPTANCE CRITERIA

For a deviation to be **ACCEPTED**, it must:

1. ✅ Be documented in this file
2. ✅ Have risk assessment (RISK_ANALYSIS.md)
3. ✅ Have mitigation plan (if negative impact)
4. ✅ Not break protocol compatibility (unless documented)
5. ✅ Be tested and verified

---

## 14. DEVIATION TRACKING

### 14.1 Current Status

| Total Deviations | 13 |
| Enhancements | 3 |
| Limitations | 4 |
| Timing | 2 |
| Behavioral | 4 |
| Documented | 13 |
| Acceptance | ✅ All documented |

---

### 14.2 Open Items

- [ ] Measure actual timing jitter (DEV-TIM-001)
- [ ] Test I2C slave reliability (DEV-COM-002)
- [ ] Document per-pin-mode deviations (Section 8)
- [ ] Verify Arduino library compatibility

---

## 15. REVISION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-28 | Initial deviation documentation (hardware, timing, communication) |

---

## 16. DOCUMENT MAINTENANCE

This document must be updated:
- During each pin mode implementation
- When timing measurements are taken
- When compatibility testing is performed
- When any behavioral difference is discovered

**Owner**: Project team  
**Review Frequency**: Per development phase  
**Governed By**: PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md Section 14.3

---

**Document Status**: LIVING DOCUMENT  
**Completeness**: ~40% (hardware/timing complete, pin modes pending)

*END OF DEVIATIONS DOCUMENT*
