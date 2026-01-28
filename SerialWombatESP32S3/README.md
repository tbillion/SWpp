# Serial Wombat ESP32-S3-N16R8 Port

**Target Hardware**: ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM)  
**Source Firmware**: Serial Wombat 18AB (PIC24FJ256GA702)  
**Project Status**: Phase 1 Complete - Planning & Documentation ✅  
**Contract Status**: 100% Compliant (60/60 requirements)

---

## 🚀 Quick Start

**For AI Agents**: Start by reading **AI_PROGRESS_TRACKER.md** - it contains exact next steps and current project state.

**For Developers**: Start with **docs/START_HERE.md** for an overview, then read the binding contract.

---

## 📁 Repository Structure

```
SerialWombatESP32S3/
│
├── AI_PROGRESS_TRACKER.md          ← CRITICAL: Current state & next steps
├── README.md                        ← This file
│
├── docs/                            ← All planning & specifications
│   ├── PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md  ← BINDING CONTRACT (immutable)
│   ├── CONTRACT_COMPLIANCE_STATUS.md            ← 100% compliance verification
│   ├── PIN_MAPPING.md                           ← GPIO safety & mapping (22 pins)
│   ├── TIMING_MODEL.md                          ← FreeRTOS architecture
│   ├── I2C_IMPLEMENTATION.md                    ← 3-tier I2C strategy
│   ├── DEVIATIONS_FROM_SW18AB.md                ← 13 documented differences
│   ├── RISK_ANALYSIS.md                         ← 23 risks & mitigations
│   ├── START_HERE.md                            ← Navigation guide
│   ├── PLANNING_SUMMARY.md                      ← Executive summary
│   ├── PORTING_CONSIDERATIONS.md                ← Original Q&A (superseded)
│   └── ESP32_S3_PORT_ROADMAP_DRAFT.md          ← Draft roadmap (superseded)
│
├── main/                            ← Implementation (Phase 2+)
│   ├── hw_abstraction/              ← ESP32 hardware abstraction layer
│   ├── core/                        ← Core firmware (from SW18AB)
│   ├── pin_modes/                   ← 40+ pin modes (from SW18AB)
│   └── common/                      ← Shared code (symlinks to ../SerialWombatCommon)
│
├── CMakeLists.txt                   ← ESP-IDF build (Phase 2)
├── platformio.ini                   ← PlatformIO config (Phase 2)
├── sdkconfig.defaults               ← ESP-IDF settings (Phase 2)
└── partitions.csv                   ← Flash layout (Phase 2)
```

---

## 📋 Project Phases

### ✅ Phase 1: Planning & Documentation (COMPLETE)
- [x] Binding contract established
- [x] 100% contract compliance verified
- [x] Risk analysis complete (23 risks, all mitigated)
- [x] Pin mapping defined (22 safe GPIOs)
- [x] Timing architecture specified (FreeRTOS dual-core)
- [x] I2C implementation designed (3-tier approach)
- [x] All deviations documented (13 items)

### ⏸️ Phase 2: Hardware Abstraction Layer (NEXT)
- [ ] Directory structure created
- [ ] Build system configured (ESP-IDF + PlatformIO)
- [ ] GPIO abstraction implemented
- [ ] Timer abstraction implemented
- [ ] UART abstraction implemented
- [ ] I2C slave abstraction implemented
- [ ] ADC abstraction implemented
- [ ] DMA/high-speed I/O implemented
- [ ] System initialization implemented

### ⏹️ Phase 3: Core Firmware Port
- [ ] Protocol handler ported
- [ ] Pin register management ported
- [ ] Main loop adapted for FreeRTOS
- [ ] Supervisor task implemented

### ⏹️ Phase 4: Pin Modes Porting (40+ modes)
- [ ] Basic modes (Digital I/O, ADC, PWM)
- [ ] Communication modes (UART, I2C)
- [ ] Input modes (Counter, Encoder, Pulse)
- [ ] Output modes (Servo, H-Bridge)
- [ ] Advanced modes (WS2812, VGA, etc.)

### ⏹️ Phase 5: Build System & Testing
- [ ] ESP-IDF build validated
- [ ] PlatformIO build validated
- [ ] Arduino library compatibility tested
- [ ] Protocol conformance tests passed

### ⏹️ Phase 6: Documentation & Validation
- [ ] Installation guide complete
- [ ] Architecture documentation complete
- [ ] User guide complete
- [ ] Final validation complete

---

## 🎯 Key Technical Decisions

| Aspect | Decision | Reference |
|--------|----------|-----------|
| **Pins** | 22 safe GPIOs (max coverage) | PIN_MAPPING.md |
| **Timing** | FreeRTOS + supervisor task | TIMING_MODEL.md |
| **I2C** | HW-first, SW fallback (same pins) | I2C_IMPLEMENTATION.md |
| **DMA** | Software baseline, HW optional | TIMING_MODEL.md |
| **ADC** | 18 channels (10 ADC1, 8 ADC2) | PIN_MAPPING.md |
| **UART** | Hardware only, configurable | Contract Section 4 |
| **Bootloader** | ESP-IDF standard (no custom) | Contract Section 10 |
| **WiFi/BLE** | Stubs only (Phase 1) | Contract Section 11 |

---

## 📖 Essential Documents

### Must Read (In Order)

1. **AI_PROGRESS_TRACKER.md** - Current state & exact next steps
2. **docs/START_HERE.md** - Project overview and navigation
3. **docs/PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md** - THE binding contract (immutable)
4. **docs/CONTRACT_COMPLIANCE_STATUS.md** - 100% compliance verification

### Technical Specifications

- **docs/PIN_MAPPING.md** - GPIO safety rules, 22 pins mapped
- **docs/TIMING_MODEL.md** - FreeRTOS dual-core architecture
- **docs/I2C_IMPLEMENTATION.md** - 3-tier reliability approach
- **docs/DEVIATIONS_FROM_SW18AB.md** - 13 documented differences
- **docs/RISK_ANALYSIS.md** - 23 risks with mitigations

---

## 🔧 Development Setup (Phase 2+)

### Prerequisites
- ESP-IDF 5.1 or later
- PlatformIO (optional)
- Python 3.7+
- Git

### Build Instructions (Coming in Phase 2)
```bash
# ESP-IDF method
cd SerialWombatESP32S3
idf.py build

# PlatformIO method
pio run
```

### Flash Instructions (Coming in Phase 2)
```bash
# ESP-IDF method
idf.py flash monitor

# PlatformIO method
pio run -t upload
pio device monitor
```

---

## 🛡️ Contract Compliance

**100% Compliant** with all 60 requirements:
- ✅ Governing Principles (6/6)
- ✅ Pin Architecture (7/7)
- ✅ UART (5/5)
- ✅ I2C (6/6)
- ✅ Timing (5/5)
- ✅ DMA (4/4)
- ✅ ADC (3/3)
- ✅ Storage (5/5)
- ✅ Bootloader (3/3)
- ✅ WiFi/BLE (3/3)
- ✅ Testing (5/5)
- ✅ Documentation (4/4)
- ✅ Standards (3/3)
- ✅ Versioning (1/1)

See **docs/CONTRACT_COMPLIANCE_STATUS.md** for details.

---

## ⚠️ Risk Management

**23 risks identified** across 8 categories:
- 5 CRITICAL (all mitigated to LOW)
- 7 HIGH (3 remain MEDIUM after mitigation)
- 7 MEDIUM (all remain MEDIUM)
- 4 LOW (all remain LOW)

All risks have documented mitigation plans and traceability.

See **docs/RISK_ANALYSIS.md** for details.

---

## 📊 Standards Alignment

This project follows practices from:
- **IEC 62304 / EN 62304** - Software lifecycle discipline
- **ISO 14971** - Risk identification and mitigation
- **ISO 13485** - Traceability and verification

Note: These are process guidelines, not certification claims.

---

## 🔗 External Resources

- **ESP-IDF Docs**: https://docs.espressif.com/projects/esp-idf/
- **Serial Wombat Arduino Library**: https://broadwellconsultinginc.github.io/SerialWombatArdLib/
- **Serial Wombat Website**: https://www.SerialWombat.com
- **Original Firmware Source**: ../SerialWombat18A_18B/SerialWombat18A_18B.X/

---

## 📞 Getting Started (Step-by-Step)

### For AI Agents
1. Read **AI_PROGRESS_TRACKER.md** (current state)
2. Read **docs/PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md** (binding contract)
3. Follow exact steps in AI_PROGRESS_TRACKER.md
4. Update AI_PROGRESS_TRACKER.md after EVERY work session

### For Human Developers
1. Read **docs/START_HERE.md** (overview)
2. Read **docs/PORTING_CONTRACT_ESP32_SERIAL_WOMBAT.md** (requirements)
3. Review **docs/PIN_MAPPING.md** (hardware constraints)
4. Check **AI_PROGRESS_TRACKER.md** (current progress)
5. Follow Phase 2 steps to begin implementation

---

## 🎓 Key Principles (From Contract)

1. **Stability over cleverness**
2. **Hardware peripherals over bit-banging**
3. **No pin choice may endanger boot, flash, USB, JTAG, or strapping**
4. **All deviations from SW18AB must be documented**
5. **If uncertain, choose the safest working solution**
6. **No irreversible operations unless provably safe**

---

## 📝 License

This port maintains the MIT License from the original Serial Wombat firmware, compatible with ESP-IDF's Apache 2.0 license.

---

## 🏁 Current Status Summary

**Phase**: 1 of 6 (Planning) - COMPLETE ✅  
**Next**: Phase 2 (HAL Implementation)  
**Blockers**: None  
**Ready**: To begin implementation  

**For detailed status, see AI_PROGRESS_TRACKER.md**

---

*Last Updated: 2026-01-28*  
*Project Start: 2026-01-27*  
*Estimated Completion: 84-122 hours from start*
