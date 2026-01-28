# ESP32-S3-N16R8 Pin Mapping for Serial Wombat Port

**Target Device**: ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM)  
**Document Status**: Authoritative Reference  
**Governed By**: PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md  
**Last Updated**: 2026-01-28

---

## 1. EXECUTIVE SUMMARY

This document defines the GPIO pin mapping strategy for the Serial Wombat ESP32-S3 port. All pin selections prioritize **boot safety**, **peripheral availability**, and **maximum feature coverage** without artificial limits.

**Key Principle**: Expose every GPIO that can safely support Serial Wombat pin modes without destabilizing boot, USB, JTAG, flash, or strapping behavior.

---

## 2. ESP32-S3 GPIO OVERVIEW

### 2.1 Total GPIO Availability

ESP32-S3 provides **45 GPIOs** (GPIO 0-48, with gaps).

However, not all are safe for general use:

| GPIO Range | Count | Notes |
|------------|-------|-------|
| GPIO 0-21 | 22 | Mixed availability (strapping, USB, ADC) |
| GPIO 26-48 | 16 | Flash/PSRAM conflicts on N16R8 variant |

**Result**: Approximately **22-30 GPIOs** are safely usable depending on configuration.

---

## 3. PIN EXCLUSION RULES (MANDATORY)

### 3.1 Strapping Pins (EXCLUDED from Serial Wombat use)

| GPIO | Function | Why Excluded |
|------|----------|--------------|
| **GPIO 0** | Boot mode select | Pulled high/low affects boot behavior |
| **GPIO 3** | JTAG_TMS | USB-JTAG conflicts, boot sensitivity |
| **GPIO 45** | VDD_SPI voltage select | Strapping pin, flash voltage |
| **GPIO 46** | ROM message control | Strapping pin, boot messages |

**Action**: ❌ Never expose these as Serial Wombat pins.

---

### 3.2 Flash and PSRAM Pins (EXCLUDED on N16R8 variant)

The ESP32-S3-N16R8 uses Octal SPI for PSRAM and Quad SPI for flash:

| GPIO Range | Function | Why Excluded |
|------------|----------|--------------|
| **GPIO 26-32** | PSRAM data lines | Octal PSRAM required by N16R8 |
| **GPIO 33-37** | PSRAM control | CS, CLK signals for PSRAM |

**Action**: ❌ Never expose these as Serial Wombat pins on N16R8.

---

### 3.3 USB-OTG Pins (CONDITIONALLY EXCLUDED)

| GPIO | Function | Status |
|------|----------|--------|
| **GPIO 19** | USB_D- | ⚠️ Usable if USB-OTG disabled |
| **GPIO 20** | USB_D+ | ⚠️ Usable if USB-OTG disabled |

**Decision**: 
- **Phase 1**: ❌ **EXCLUDE** (USB-UART is primary console/programming interface)
- **Future**: ✅ Allow if user explicitly disables USB-OTG in sdkconfig

**Rationale**: Losing USB console is a major usability risk. Safety first.

---

### 3.4 JTAG Pins (CONDITIONALLY EXCLUDED)

| GPIO | Function | Status |
|------|----------|--------|
| **GPIO 39** | JTAG_TDI | ⚠️ Usable if JTAG disabled |
| **GPIO 40** | JTAG_TDO | ⚠️ Usable if JTAG disabled |
| **GPIO 41** | JTAG_TCK | ⚠️ Usable if JTAG disabled |
| **GPIO 42** | JTAG_TMS | ⚠️ Usable if JTAG disabled |

**Decision**:
- **Phase 1**: ✅ **INCLUDE** with documentation warning
- Most users don't use JTAG debugging
- Document how to disable JTAG if needed

**Rationale**: Expand usable pins, but warn users about JTAG conflict.

---

## 4. SAFE GPIO ALLOCATION

### 4.1 Primary Safe GPIOs (Always Available)

| GPIO | Features | ADC | Notes |
|------|----------|-----|-------|
| **GPIO 1** | General I/O, ADC | ADC1_CH0 | Safe, ADC-capable |
| **GPIO 2** | General I/O, ADC | ADC1_CH1 | Safe, ADC-capable |
| **GPIO 4** | General I/O, ADC | ADC1_CH3 | Safe, ADC-capable |
| **GPIO 5** | General I/O, ADC | ADC1_CH4 | Safe, ADC-capable |
| **GPIO 6** | General I/O, ADC | ADC1_CH5 | Safe, ADC-capable |
| **GPIO 7** | General I/O, ADC | ADC1_CH6 | Safe, ADC-capable |
| **GPIO 8** | General I/O, ADC | ADC1_CH7 | Safe, ADC-capable, default I2C SDA |
| **GPIO 9** | General I/O, ADC | ADC1_CH8 | Safe, ADC-capable, default I2C SCL |
| **GPIO 10** | General I/O, ADC | ADC1_CH9 | Safe, ADC-capable |
| **GPIO 11** | General I/O, ADC | ADC2_CH0 | ⚠️ WiFi conflict (Phase 1: safe) |
| **GPIO 12** | General I/O, ADC | ADC2_CH1 | ⚠️ WiFi conflict (Phase 1: safe) |
| **GPIO 13** | General I/O, ADC | ADC2_CH2 | ⚠️ WiFi conflict (Phase 1: safe) |
| **GPIO 14** | General I/O, ADC | ADC2_CH3 | ⚠️ WiFi conflict (Phase 1: safe) |
| **GPIO 15** | General I/O, ADC | ADC2_CH4 | ⚠️ WiFi conflict (Phase 1: safe) |
| **GPIO 16** | General I/O, ADC | ADC2_CH5 | ⚠️ WiFi conflict (Phase 1: safe) |
| **GPIO 17** | General I/O, ADC | ADC2_CH6 | ⚠️ WiFi conflict (Phase 1: safe) |
| **GPIO 18** | General I/O, ADC | ADC2_CH7 | ⚠️ WiFi conflict (Phase 1: safe) |
| **GPIO 21** | General I/O | - | Safe, no ADC |

**Count**: 18 GPIOs (10 on ADC1, 8 on ADC2, 1 digital-only)

---

### 4.2 UART-Reserved GPIOs

| GPIO | Function | Configurable | Notes |
|------|----------|--------------|-------|
| **GPIO 43** | UART0_TX | No (default) | USB-UART bridge TX |
| **GPIO 44** | UART0_RX | No (default) | USB-UART bridge RX |
| **GPIO 47** | UART1_TX | Yes | User-selectable protocol UART |
| **GPIO 48** | UART1_RX | Yes | User-selectable protocol UART |

**Status**: Reserved for UART, not exposed as general Serial Wombat pins.

---

### 4.3 JTAG GPIOs (Usable with Warning)

| GPIO | Features | Notes |
|------|----------|-------|
| **GPIO 39** | JTAG_TDI | ✅ Usable, warn about JTAG conflict |
| **GPIO 40** | JTAG_TDO | ✅ Usable, warn about JTAG conflict |
| **GPIO 41** | JTAG_TCK | ✅ Usable, warn about JTAG conflict |
| **GPIO 42** | JTAG_TMS | ✅ Usable, warn about JTAG conflict |

**Count**: 4 GPIOs (no ADC, digital I/O only)

---

## 5. FINAL PIN ALLOCATION

### 5.1 Serial Wombat Pin Mapping (Maximum Safe Coverage)

#### Legacy Pins (SW18AB Compatibility: Pins 0-17)

| SW Pin | ESP32 GPIO | Features | Notes |
|--------|------------|----------|-------|
| **0** | GPIO 1 | ADC1_CH0, Digital I/O | Safe, ADC-capable |
| **1** | GPIO 2 | ADC1_CH1, Digital I/O | Safe, ADC-capable |
| **2** | GPIO 4 | ADC1_CH3, Digital I/O | Safe, ADC-capable |
| **3** | GPIO 5 | ADC1_CH4, Digital I/O | Safe, ADC-capable |
| **4** | GPIO 6 | ADC1_CH5, Digital I/O | Safe, ADC-capable |
| **5** | GPIO 7 | ADC1_CH6, Digital I/O | Safe, ADC-capable |
| **6** | GPIO 8 | ADC1_CH7, I2C SDA | Safe, ADC + I2C |
| **7** | GPIO 9 | ADC1_CH8, I2C SCL | Safe, ADC + I2C |
| **8** | GPIO 10 | ADC1_CH9, Digital I/O | Safe, ADC-capable |
| **9** | GPIO 11 | ADC2_CH0, Digital I/O | ⚠️ WiFi conflict (future) |
| **10** | GPIO 12 | ADC2_CH1, Digital I/O | ⚠️ WiFi conflict (future) |
| **11** | GPIO 13 | ADC2_CH2, Digital I/O | ⚠️ WiFi conflict (future) |
| **12** | GPIO 14 | ADC2_CH3, Digital I/O | ⚠️ WiFi conflict (future) |
| **13** | GPIO 15 | ADC2_CH4, Digital I/O | ⚠️ WiFi conflict (future) |
| **14** | GPIO 16 | ADC2_CH5, Digital I/O | ⚠️ WiFi conflict (future) |
| **15** | GPIO 17 | ADC2_CH6, Digital I/O | ⚠️ WiFi conflict (future) |
| **16** | GPIO 18 | ADC2_CH7, Digital I/O | ⚠️ WiFi conflict (future) |
| **17** | GPIO 21 | Digital I/O only | Safe, no ADC |

**Count**: 18 pins (matching SW18AB name, exceeding 18AB physical count)

---

#### Extended Pins (ESP32-Specific: Pins 18-21)

| SW Pin | ESP32 GPIO | Features | Notes |
|--------|------------|----------|-------|
| **18** | GPIO 39 | Digital I/O, JTAG_TDI | ⚠️ JTAG conflict if used |
| **19** | GPIO 40 | Digital I/O, JTAG_TDO | ⚠️ JTAG conflict if used |
| **20** | GPIO 41 | Digital I/O, JTAG_TCK | ⚠️ JTAG conflict if used |
| **21** | GPIO 42 | Digital I/O, JTAG_TMS | ⚠️ JTAG conflict if used |

**Count**: 4 extended pins (digital I/O only, no ADC)

---

### 5.2 Total Pin Count Summary

| Category | Count | Notes |
|----------|-------|-------|
| **Legacy Pins (0-17)** | 18 | SW18AB compatibility |
| **Extended Pins (18-21)** | 4 | ESP32-specific |
| **Total Serial Wombat Pins** | **22** | Maximum safe coverage |
| **ADC-Capable Pins** | 18 | 10 on ADC1, 8 on ADC2 |
| **Digital-Only Pins** | 4 | JTAG pins, no ADC |

---

## 6. I2C ADDRESS SELECT PINS

Per contract requirement: **4 sequential GPIOs for I2C address selection**.

### 6.1 Selected Pins (Physically Adjacent if Possible)

| Addr Pin | ESP32 GPIO | Reason |
|----------|------------|--------|
| **A0** | GPIO 11 | Sequential, safe |
| **A1** | GPIO 12 | Sequential, safe |
| **A2** | GPIO 13 | Sequential, safe |
| **A3** | GPIO 14 | Sequential, safe |

**Address Range**: 0x00 - 0x0F (16 addresses)

**Address Calculation**:
```
I2C_Address = Base_Address + (A3<<3 | A2<<2 | A1<<1 | A0)
```

**Default Base Address**: 0x6B (matching SW18AB default)

**Note**: These pins are also usable as general Serial Wombat pins when not in I2C address select mode.

---

## 7. PERIPHERAL FEATURE MATRIX

### 7.1 ADC Channels

| ADC | GPIO Pins | Count | SW Pins | WiFi Conflict |
|-----|-----------|-------|---------|---------------|
| **ADC1** | 1,2,4-10 | 10 | 0-8 | ❌ No conflict |
| **ADC2** | 11-18 | 8 | 9-16 | ⚠️ Yes (future) |

**ADC Resolution**: 12-bit (vs 10-bit on SW18AB PIC24)  
**Scaling**: SW protocol expects 0-65535, so 12-bit → 16-bit scaling required

---

### 7.2 PWM (LEDC) Channels

ESP32-S3 has **8 LEDC channels**, all GPIOs support PWM via routing.

**All 22 Serial Wombat pins support PWM.**

---

### 7.3 UART

| UART | Default GPIOs | Configurable | Purpose |
|------|---------------|--------------|---------|
| **UART0** | TX=43, RX=44 | No | Console, diagnostics |
| **UART1** | TX=47, RX=48 | Yes | Protocol transport |
| **UART2** | Any GPIO | Yes | Future pin modes |

---

### 7.4 I2C

| I2C | Default GPIOs | Purpose |
|-----|---------------|---------|
| **I2C0** | SDA=8, SCL=9 | Serial Wombat slave |
| **I2C1** | User-defined | Future pin modes (I2C controller) |

---

### 7.5 SPI

| SPI | Purpose | GPIOs |
|-----|---------|-------|
| **SPI2** | Available | Configurable |
| **SPI3** | Flash (internal) | Reserved |

**Note**: SPI pins can be used for Serial Wombat pin modes when not in SPI mode.

---

## 8. PIN CAPABILITY SUMMARY

### 8.1 Per-Pin Capabilities

| SW Pin | GPIO | ADC | PWM | I2C | UART | JTAG | Notes |
|--------|------|-----|-----|-----|------|------|-------|
| 0 | 1 | ✅ | ✅ | - | - | - | ADC1, safe |
| 1 | 2 | ✅ | ✅ | - | - | - | ADC1, safe |
| 2 | 4 | ✅ | ✅ | - | - | - | ADC1, safe |
| 3 | 5 | ✅ | ✅ | - | - | - | ADC1, safe |
| 4 | 6 | ✅ | ✅ | - | - | - | ADC1, safe |
| 5 | 7 | ✅ | ✅ | - | - | - | ADC1, safe |
| 6 | 8 | ✅ | ✅ | SDA | - | - | ADC1, I2C default |
| 7 | 9 | ✅ | ✅ | SCL | - | - | ADC1, I2C default |
| 8 | 10 | ✅ | ✅ | - | - | - | ADC1, safe |
| 9 | 11 | ⚠️ | ✅ | - | - | - | ADC2, WiFi conflict |
| 10 | 12 | ⚠️ | ✅ | - | - | - | ADC2, WiFi conflict |
| 11 | 13 | ⚠️ | ✅ | - | - | - | ADC2, WiFi conflict |
| 12 | 14 | ⚠️ | ✅ | - | - | - | ADC2, WiFi conflict |
| 13 | 15 | ⚠️ | ✅ | - | - | - | ADC2, WiFi conflict |
| 14 | 16 | ⚠️ | ✅ | - | - | - | ADC2, WiFi conflict |
| 15 | 17 | ⚠️ | ✅ | - | - | - | ADC2, WiFi conflict |
| 16 | 18 | ⚠️ | ✅ | - | - | - | ADC2, WiFi conflict |
| 17 | 21 | ❌ | ✅ | - | - | - | Digital only |
| 18 | 39 | ❌ | ✅ | - | - | ⚠️ | JTAG conflict |
| 19 | 40 | ❌ | ✅ | - | - | ⚠️ | JTAG conflict |
| 20 | 41 | ❌ | ✅ | - | - | ⚠️ | JTAG conflict |
| 21 | 42 | ❌ | ✅ | - | - | ⚠️ | JTAG conflict |

---

## 9. BOOT SAFETY VALIDATION

### 9.1 Boot Mode Selection Truth Table

| GPIO 0 | GPIO 45 | GPIO 46 | Boot Mode |
|--------|---------|---------|-----------|
| 0 | X | X | Download mode |
| 1 | 0 | X | SPI boot (1.8V) |
| 1 | 1 | X | SPI boot (3.3V) |

**Our Pin Selection Impact**: ✅ None of our selected pins affect boot mode.

---

### 9.2 Pre-Boot Pin State

All selected GPIOs default to **high-impedance input** on boot, which is safe.

---

## 10. WIRING RECOMMENDATIONS

### 10.1 Voltage Levels

- **ESP32-S3 I/O**: 3.3V (NOT 5V tolerant)
- **SW18AB PIC24**: 5V tolerant

**Warning**: ⚠️ **Do NOT apply 5V signals to ESP32-S3 GPIOs**

**Recommendation**: Use level shifters for 5V interfacing.

---

### 10.2 Current Limits

- **Per GPIO**: 40mA maximum (configured to 20mA default)
- **Total chip**: 200mA maximum across all GPIOs

**Recommendation**: Use external drivers for servos, motors, high-current loads.

---

## 11. DEVIATIONS FROM SW18AB

### 11.1 Pin Count

- **SW18AB**: 18 physical pins (20 with virtual pins)
- **ESP32 Port**: 22 physical pins

**Impact**: More pins available, fully backward compatible.

---

### 11.2 ADC Resolution

- **SW18AB**: 10-bit ADC (0-1023, scaled to 0-65535)
- **ESP32**: 12-bit ADC (0-4095, scaled to 0-65535)

**Impact**: Higher resolution, better accuracy.

---

### 11.3 ADC2/WiFi Conflict

- **SW18AB**: N/A (no WiFi)
- **ESP32**: ADC2 cannot be used with WiFi

**Impact**: In future WiFi phases, ADC2 pins (9-16) will be unavailable for ADC function.

---

### 11.4 Pin Voltage

- **SW18AB**: 5V tolerant
- **ESP32**: 3.3V only

**Impact**: Requires level shifters for 5V interfacing.

---

## 12. DOCUMENTATION CROSS-REFERENCES

- **Contract**: PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md (Section 3: Pin Architecture)
- **Risk Analysis**: RISK_ANALYSIS.md (RISK-HW-001, RISK-HW-002)
- **Architecture**: (To be created: ARCHITECTURE.md)

---

## 13. REVISION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-28 | Initial pin mapping based on binding contract |

---

**Document Status**: AUTHORITATIVE  
**Modification Policy**: Updates require risk assessment and contract compliance check

*END OF PIN MAPPING DOCUMENT*
