# Serial Wombat → ESP32-S3 Porting Contract

**Target**: ESP32-S3-N16R8 (initial), extensible to other ESP32 variants  
**Protocol**: Serial Wombat 18AB  
**Status**: BINDING CONTRACT  
**Modification Policy**: This document is immutable once accepted. Addenda must be separate files.  
**Date Established**: 2026-01-28  
**Authority**: This document supersedes all prior planning documents and assumptions.

---

## 1. PURPOSE AND NON-GOALS

### 1.1 Purpose

This contract defines the mandatory technical, architectural, behavioral, and documentation requirements for porting the Serial Wombat 18AB firmware to ESP32-class hardware.

The goal is **protocol parity**, **behavioral equivalence**, and **improved stability** without sacrificing compatibility.

### 1.2 Non-Goals (Explicit Exclusions)

The following are **out of scope for Phase 1**:

- WiFi, BLE, MQTT, Web UI, TCP bridges
- Custom bootloader compatible with SW18AB update tools
- Experimental DMA techniques that risk instability
- Any feature that alters Serial Wombat observable behavior

**Future features must be planned but stubbed, not implemented.**

---

## 2. GOVERNING PRINCIPLES (NON-NEGOTIABLE)

1. **Stability over cleverness**
2. **Hardware peripherals over bit-banging**
3. **No pin choice may endanger boot, flash, USB, JTAG, or strapping**
4. **All deviations from SW18AB must be documented**
5. **If uncertain, choose the safest working solution**
6. **No irreversible operations** (bootloader, flash overwrite) unless provably safe

---

## 3. PIN ARCHITECTURE CONTRACT

### 3.1 Logical Pin Model

- **Preserve SW18AB logical pin numbering** for compatibility
- **Add extended pin support** for ESP32-only pins where safe

Example:
```
SW Pin 0–17  → Legacy compatibility
ESP Pin 18–N → Extended pins (documented)
```

### 3.2 Physical Pin Eligibility Rules (MUST)

A GPIO **MUST NOT** be used if it:

- Is a strapping pin
- Is required for flash/PSRAM
- Is required for USB-OTG or JTAG (unless explicitly disabled)
- Causes boot mode ambiguity
- Causes console lockout
- Is unstable across resets

### 3.3 Pin Count Strategy

**Maximum safe pin coverage**: Do not artificially cap to 18/20/30. Expose every GPIO that can safely support the requested pin-mode function without destabilizing boot/USB/JTAG/flash/straps.

### 3.4 Feature Availability

- A pin MAY expose a feature **only if the silicon natively supports it**
- ADC, PWM, interrupts, etc. must not be emulated where hardware exists
- Pins lacking a function must not advertise it

---

## 4. SERIAL INTERFACES (UART)

### 4.1 UART Usage (MUST)

- All Serial Wombat protocol traffic **must use a hardware UART**
- **UART0** (USB-UART) must be available by default for setup
- **UART1** must be user-selectable as the protocol interface
- **Daisy-chain UART** must be stubbed, not implemented

### 4.2 Configuration Philosophy

- **Default behavior**: minimal friction
- **Advanced behavior**: fully configurable

| Parameter | Default | Configurable |
|-----------|---------|--------------|
| Baud rate | 115200 | Yes |
| Transport UART | UART0 | Yes |
| ASCII/Binary | Binary | Yes |

### 4.3 Baud Rate Requirements

- **Default**: 115200 for lowest friction
- **Runtime configurable** for power users
- **Range**: 9600 to 1000000 baud (or the highest proven stable set of standard rates; document what is tested)

### 4.4 Interface Availability

- **UART0**: Yes (default for initial setup/config and diagnostics)
- **UART1**: Yes (user-selectable as protocol transport)
- **Simultaneous operation**: Yes for diagnostics + protocol, but protocol should run on one selected UART at a time (unless future daisy-chain is implemented)

---

## 5. I2C CONTRACT

### 5.1 Hardware First Rule (MUST)

- I2C must **always target hardware I2C controllers first**
- Software I2C is **fallback only, never default**

### 5.2 Pin Stability Rule

- Hardware and software I2C **must use the same physical pins**
- Users must **never rewire** when switching modes

### 5.3 Multiple Controllers

If the ESP32 target has 2 hardware I2C controllers:
- Both must be supported
- Slave mode availability must be documented

### 5.4 Address Select Pins

Exactly **4 GPIOs** must be reserved for I2C address selection.

Pins must be:
- **Sequential where possible**
- **Physically adjacent** on the target board layout
- Address mapping must match Broadwell conventions

**Pins must be documented prominently.**

### 5.5 Criticality

- **Required**: SW ecosystem uses I2C slave; must be supported

---

## 6. TIMING AND EXECUTION MODEL

### 6.1 Behavioral Parity Requirement

The ESP32 port must **not change the externally observable timing behavior** of SW18AB pin modes.

### 6.2 Execution Architecture

**Default model:**
- Hardware timer ISR → semaphore
- High-priority FreeRTOS task executes `ProcessPins()`
- RX processing occurs independently
- Watchdog-safe execution only

### 6.3 Timing Jitter Tolerance

- **Correctness and protocol compliance are critical**
- Small jitter is acceptable if it does not change observable behavior vs SW18AB
- **Measure actual jitter and document it**
- Prioritize deterministic behavior for modes that require it

### 6.4 Redundancy / Stability (MUST)

A **supervisor task MUST exist** to:
- Detect missed cycles
- Detect ISR starvation
- Detect task overruns

**Recovery behavior must be deterministic and logged.**

---

## 7. DMA / HIGH-SPEED I/O

### 7.1 Performance Target

- Achieve **equal or better performance** than SW18AB PIC hardware
- Do not exceed complexity that risks instability

### 7.2 Approved Approaches

- **Baseline**: Software-emulated sampling with bounded ISR time (Option A)
- **Optional**: Hardware-accelerated path (I2S/etc.) only if stable (Option B)

### 7.3 CPU Overhead Policy

- Acceptable if stable and does not starve other critical tasks
- **Profiling required**
- If ISR emulation is used, instrument it and enforce watchdog-safe behavior (no long ISRs; bounded time)

### 7.4 User Choice Policy

- Advanced DMA paths may be selectable
- **Defaults must always be the most stable configuration**
- All caveats must be documented per pin mode

---

## 8. ADC CONTRACT

### 8.1 ADC Requirements

- **Yes**: Enable ADC on every safe ADC-capable pin that the hardware supports without causing stability issues
- **Prefer ADC1 pins** for compatibility and reduced WiFi conflict
- ADC2 can exist but **document WiFi interaction clearly**

---

## 9. NON-VOLATILE STORAGE

### 9.1 Functional Requirements

Storage must support:
- User buffer persistence
- Calibration data
- Command capture sequences
- Configuration data

### 9.2 Implementation Rule

- Choose the method that **best matches SW18AB behavior**
- **NVS or dedicated partition** is acceptable
- **Wear leveling must be guaranteed**

### 9.3 Documentation Requirement

Any behavioral difference from SW18AB flash behavior must be documented.

---

## 10. BOOTLOADER AND UPDATES

### 10.1 Safety First Rule

- **No custom bootloader** unless corruption is provably impossible
- **Bricking risk is unacceptable**

### 10.2 Phase 1 Policy

- Use **ESP-IDF standard bootloader**
- Firmware flashing via `esptool`
- **OTA via ESP-IDF framework** (WiFi-based)

### 10.3 Future Compatibility

- SW18AB-style update compatibility may be explored later
- Only after a **non-brickable recovery path** exists

### 10.4 UART Update Policy

- UART-based update can be considered later only if it is robust and cannot brick

---

## 11. WIFI AND BLUETOOTH

### 11.1 Phase 1 Policy

- **No WiFi/BLE implementation in Phase 1**
- Provide **stubs/hooks only**

### 11.2 Compile-Time Control

- **Compile-time feature gate flags** (like SW8B feature gates)
- Runtime enable/disable may be added later, but not required for Phase 1

### 11.3 Future Phases Planned

- WiFi configuration portal
- Web API
- MQTT bridge
- OTA updates
- BLE (later if useful)

---

## 12. POWER MANAGEMENT

### 12.1 Phase 1 Policy

- **Always-on operation acceptable**
- No aggressive sleep modes that break reachability

### 12.2 Future Power Modes

Light sleep permitted **only if**:
- UART wakes reliably
- I2C wakes reliably
- Pin state machines remain correct

### 12.3 Critical Features in Sleep

In any light-sleep mode, keep:
- Pin state machines
- UART communication
- I2C communication
- Timers as needed
- GPIO interrupts required for wake

**Must wake reliably on comms/events; never become "unreachable" unexpectedly.**

---

## 13. TESTING AND VALIDATION

### 13.1 Validation Level (REQUIRED)

- **Full pin-mode parity**
- **Performance comparable to SW18AB**
- Timing-sensitive modes verified via scope/logic analyzer

### 13.2 Test Types

- **Manual** (Arduino / real hardware)
- **Automated** where feasible (repeatable test harness)
- Automated tests should include timing/performance checks for critical modes
- **Regression tests** for protocol behavior

### 13.3 Test Hardware

- Yes, can test all pin modes (logic analyzer/scope available or planned)

### 13.4 Acceptance Criteria

The port is **not accepted** unless:
- All core pin modes function
- No undocumented deviations exist
- System runs continuously without instability
- **Performance equivalent to SW18AB** (exhaustive) AND full 1:1 parity where applicable
- Any "not supported / behavior differs" must be clearly documented

---

## 14. DOCUMENTATION REQUIREMENTS

### 14.1 Format

- **Markdown only**
- **Ultra-verbose**
- **Who / What / Where / When / Why** compliant

### 14.2 Mandatory Documents

1. **ROADMAP.md** (immutable after creation; addenda allowed)
2. **PIN_MAPPING.md**
3. **DEVIATIONS_FROM_SW18AB.md**
4. **TIMING_MODEL.md**
5. **I2C_IMPLEMENTATION.md**
6. **RISK_ANALYSIS.md**
7. **INSTALLATION.md**
8. **ARCHITECTURE.md**

### 14.3 Deviation Documentation

All deviations from SW18AB behavior, timing, or mode semantics must be documented in a **"Deviations" section per mode and per subsystem**.

### 14.4 Change Control

- No silent behavior changes
- All differences require documentation

---

## 15. STANDARDS ALIGNMENT (ENGINEERING DISCIPLINE)

The project must align with practices from:

- **IEC 62304 / EN 62304** (software lifecycle discipline)
- **ISO 14971** (risk identification and mitigation)
- **ISO 13485** (traceability and verification mindset)

**This is process guidance, not certification.**

### 15.1 Risk Management

- Identify risks per subsystem
- Document mitigation strategies
- Maintain traceability matrix

### 15.2 Verification Artifacts

- Test plans
- Test results
- Traceability documentation
- Risk analysis results

---

## 16. BOARD VARIANTS

### 16.1 Phase 1 Target

- **Primary target**: ESP32-S3-N16R8

### 16.2 Future Extensibility

- Architecture must support other ESP32 variants later via configuration/HAL pin maps **without rewriting core logic**

---

## 17. VERSIONING

### 17.1 Version Format (MANDATORY)

Versioning must reflect lineage:

```
SW18AB-vX.Y.Z-esp32s3
```

Example: `SW18AB-v2.1.3-esp32s3`

**Independent versioning is not permitted in Phase 1.**

---

## 18. TIMELINE AND DELIVERY

### 18.1 Timeline

- **ASAP** (prompt-driven rapid iteration), but **not at the expense of stability**

### 18.2 Phased Delivery

- **Yes**. Phased delivery acceptable.
- Create a single ultra-verbose **ROADMAP.md** with phases and acceptance criteria
- Once created, **do not modify it** (addendum files allowed if absolutely necessary)

---

## 19. LICENSE

### 19.1 License Policy

- **No concerns**
- Keep **MIT** for the port, compatible with ESP-IDF Apache 2.0 usage

---

## 20. FINAL AUTHORITY CLAUSE

If any ambiguity exists:

1. **This contract overrides assumptions**
2. **Stability overrides performance**
3. **Documentation overrides intuition**
4. **Safety overrides convenience**

---

## 21. IMPLEMENTATION DECISIONS MATRIX

Based on the authoritative user responses to all 28 questions:

| Question | Decision | Rationale |
|----------|----------|-----------|
| **Q1: Pin count** | Max safe coverage | Expose all safe GPIOs, not artificially capped |
| **Q2: UART/I2C pins** | Hardware peripherals, configurable | UART0 default, UART1 selectable, hardware I2C |
| **Q3: ADC** | All safe ADC pins | ADC1 preferred, document ADC2/WiFi conflicts |
| **Q4: DMA** | Sw emulation baseline + optional HW | Start stable, allow high-perf mode if proven |
| **Q5: CPU overhead** | Acceptable if profiled | Bounded ISR time, watchdog-safe |
| **Q6: Timing** | FreeRTOS + supervisor | ISR→semaphore, health monitoring required |
| **Q7: Jitter** | Measured & documented | Small jitter OK if behavior unchanged |
| **Q8: I2C** | Hardware first, sw fallback same pins | Never change wiring |
| **Q9: I2C critical** | Required | Ecosystem depends on it |
| **Q10: Storage** | Match SW18AB behavior | NVS or partition with wear leveling |
| **Q11: NV features** | All (buffer, cal, capture, config) | Complete support required |
| **Q12: UART interfaces** | UART0 + UART1, simultaneous for diag | Configurable transport |
| **Q13: Baud rate** | 115200 default, 9600-1M configurable | Document tested rates |
| **Q14: WiFi/BLE Phase 1** | No, stubs only | Future: portal, API, MQTT, OTA |
| **Q15: WiFi/BLE optional** | Compile-time flags | Like SW8B feature gates |
| **Q16: Bootloader** | ESP-IDF standard only | No custom unless provably safe |
| **Q17: OTA** | Yes, WiFi via ESP-IDF | UART later if robust |
| **Q18: Low-power** | Not critical Phase 1 | Always-on acceptable |
| **Q19: Sleep features** | All comms + state machines | Never unreachable |
| **Q20: Test hardware** | Yes, all modes | Logic analyzer/scope available |
| **Q21: Testing** | Manual + automated | Timing/performance checks |
| **Q22: Validation** | Exhaustive + 1:1 parity | Document all deviations |
| **Q23: Timeline** | ASAP but stable | No compromise on stability |
| **Q24: Phased** | Yes | Ultra-verbose ROADMAP.md (immutable) |
| **Q25: Board variants** | ESP32-S3-N16R8 Phase 1 | Extensible HAL architecture |
| **Q26: Docs** | Ultra-verbose MD | GitHub-friendly, Doxygen optional |
| **Q27: License** | MIT (port) + Apache 2.0 (ESP-IDF) | No concerns |
| **Q28: Versioning** | SW18AB-vX.Y.Z-esp32s3 | Preserve lineage |

---

## 22. CONTRACT ACCEPTANCE

This contract is **BINDING** and **AUTHORITATIVE**.

All implementation work must conform to this contract.

Deviations require documented justification and risk assessment.

---

**END OF PORTING CONTRACT**

*This document is immutable. All changes require addenda in separate files.*
