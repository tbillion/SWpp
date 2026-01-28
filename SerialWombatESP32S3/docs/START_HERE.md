# Serial Wombat ESP32-S3-N16R8 Port - Getting Started

**Status**: PLANNING PHASE COMPLETE - AWAITING USER REVIEW

---

## What Has Been Done

I've completed the initial analysis and planning phase for porting the Serial Wombat firmware to ESP32-S3-N16R8. Here's what's ready for your review:

### 📋 Documents Created

1. **ESP32_S3_PORT_ROADMAP.md** - The Master Plan
   - Complete step-by-step porting roadmap
   - 8 phases from preparation to final validation
   - Detailed task breakdown for each phase
   - Success criteria and timeline estimates
   - **THIS IS THE SINGLE SOURCE OF TRUTH** - do not modify during development

2. **PORTING_CONSIDERATIONS.md** - Decisions Needed From You
   - 28 critical questions requiring your input
   - Technical design decisions (DMA, timing, I2C)
   - Feature scope decisions (WiFi/BLE, OTA, power modes)
   - Testing and validation strategy
   - **YOUR REVIEW REQUIRED BEFORE PROCEEDING**

3. **This file (START_HERE.md)** - Quick navigation guide

---

## What You Need To Do Now

### Step 1: Review the Roadmap
**File**: `ESP32_S3_PORT_ROADMAP.md`
**Time**: 15-20 minutes
**Purpose**: Understand the overall approach and project structure

**Key sections to review:**
- Project Overview (source vs target hardware)
- Repository Structure (where files will go)
- Phase breakdown (8 phases total)
- Success Criteria (what "done" looks like)

### Step 2: Answer the Questions
**File**: `PORTING_CONSIDERATIONS.md`
**Time**: 30-60 minutes
**Purpose**: Guide critical technical decisions

**Important questions (see full document for all 28):**

**Must Answer:**
- Q1: How many pins? (20 recommended)
- Q4: DMA implementation approach (Software emulation recommended)
- Q6: Timing architecture (FreeRTOS semaphore recommended)
- Q8: I2C slave method (Hardware first recommended)
- Q14: WiFi/BLE in Phase 1? (No recommended)

**Should Answer:**
- Q22: Validation level required
- Q23: Timeline expectations
- Q24: Accept phased delivery?

**Can Skip (use defaults):**
- Q2, Q3, Q12, Q13: Pin/UART details (use recommended defaults)
- Q10, Q11: Storage (use NVS)
- Q16, Q17: Bootloader (use ESP-IDF standard)

**How to provide feedback:**
Edit `PORTING_CONSIDERATIONS.md` and add your responses in the "USER FEEDBACK SECTION" at the bottom, OR respond in comments/chat with your preferences.

### Step 3: Notify When Ready
Once you've reviewed both documents and provided answers to the critical questions, I'll proceed with Phase 2 (Hardware Abstraction Layer implementation).

---

## Architecture Summary

### What is Serial Wombat?
A microcontroller firmware that extends I/O capabilities via UART/I2C:
- Configurable pin modes (40+ types)
- Real-time processing (1ms update cycle)
- 8-byte packet protocol
- Pre-programmed, runtime-configured

### Source: SW18AB (PIC24FJ256GA702)
- 20 GPIO pins, 9 ADC channels
- 8KB RAM, 256KB Flash
- Bare-metal firmware
- DMA for high-speed I/O
- All features in one build

### Target: ESP32-S3-N16R8
- 45 GPIO pins (30+ usable)
- 512KB RAM, 16MB Flash, 8MB PSRAM
- FreeRTOS-based
- GDMA channels available
- All features enabled + room for more

### Why This Port?
- **Memory abundance**: No feature-gating needed (vs SW8B fragmentation)
- **Performance**: Faster CPU, more peripherals
- **Future-proof**: WiFi/BLE capabilities available
- **1:1 parity**: All SW18AB features will work identically

---

## Quick Reference

### File Locations (Current)
```
SWpp/
├── ESP32_S3_PORT_ROADMAP.md           ← Master plan (read-only)
├── PORTING_CONSIDERATIONS.md          ← YOUR ACTION REQUIRED
├── START_HERE.md                      ← This file
├── SerialWombat18A_18B/               ← Source firmware
│   └── SerialWombat18A_18B.X/
│       ├── main.c                     ← Original main loop
│       ├── protocol.c                 ← Command processing
│       └── [40+ pin mode files]
├── SerialWombat8B/                    ← Fragmented variant (reference)
└── SerialWombatCommon/                ← Shared headers/code
    ├── serialWombat.h
    ├── pinModes.h
    └── pinRegisters.h
```

### File Locations (After Port)
```
SWpp/
└── SerialWombatESP32S3/               ← NEW PORT (to be created)
    ├── README.md
    ├── INSTALLATION.md
    ├── platformio.ini
    ├── CMakeLists.txt
    ├── main/
    │   ├── hw_abstraction/            ← ESP32-specific HAL
    │   ├── core/                      ← Ported SW18AB core
    │   ├── pin_modes/                 ← Ported pin modes
    │   └── common/                    ← Symlinks to SerialWombatCommon
    └── docs/
```

---

## Technical Highlights

### Key Design Decisions (Recommended Defaults)

| Aspect | SW18AB (Original) | ESP32 Port (Recommended) |
|--------|-------------------|--------------------------|
| **Timing** | Bare-metal 1ms interrupt | FreeRTOS semaphore |
| **DMA** | Hardware circular buffers | Software emulation |
| **I2C Slave** | Hardware I2C | Hardware (with testing) |
| **Pin Count** | 20 physical | 20 (expandable to 30) |
| **ADC Channels** | 9 (10-bit) | 10+ (12-bit) |
| **Storage** | Flash | NVS |
| **WiFi/BLE** | N/A | Not in Phase 1 |
| **Build System** | MPLAB X | ESP-IDF + PlatformIO |

### Port Complexity Estimate
- **Hardware Abstraction Layer**: 16-24 hours
- **Core Firmware**: 8-12 hours (mostly copy/paste)
- **Pin Modes**: 32-48 hours (40+ modes to adapt)
- **Testing**: 8-12 hours
- **Documentation**: 6-8 hours
- **Total**: 80-120 hours

### Success Criteria
✅ All 40+ pin modes functional  
✅ Binary protocol 100% compatible  
✅ Works with existing Arduino library  
✅ 1ms timing maintained  
✅ UART and I2C communication  

---

## Common Questions

### Q: Will this work with existing Serial Wombat libraries?
**A**: Yes! The Arduino, Python, and C# libraries communicate via the same 8-byte packet protocol over UART or I2C. The ESP32 port maintains complete protocol compatibility.

### Q: Can I use WiFi/Bluetooth?
**A**: Not in Phase 1 (maintaining 1:1 parity). Can be added in future phases if desired (see Q14 in considerations document).

### Q: How is this different from SW8B?
**A**: 
- **SW8B**: Feature-gated builds due to limited memory (CH32V003)
- **SW18AB**: All features in one build (PIC24FJ256GA702)
- **ESP32 Port**: All features + abundant resources for future expansion

### Q: Can I modify the roadmap?
**A**: No. The roadmap is intentionally read-only to serve as a navigation aid if development gets complex. Any changes should be discussed and documented separately.

### Q: What if I want custom features?
**A**: Phase 1 focuses on 1:1 parity. Custom features (WiFi bridge, BLE, OTA, web interface) can be added in later phases after core functionality is validated.

### Q: Do I need special hardware?
**A**: Minimum:
- ESP32-S3-N16R8 development board
- USB cable
- Computer with ESP-IDF or PlatformIO

Recommended for testing:
- Logic analyzer
- Oscilloscope
- Various I/O devices (servos, encoders, etc.)

---

## Next Steps (After Your Review)

Once you provide feedback on `PORTING_CONSIDERATIONS.md`, I will:

1. ✅ Create `SerialWombatESP32S3/` directory structure
2. ✅ Implement hardware abstraction layer (GPIO, timers, UART, I2C, ADC)
3. ✅ Port core firmware (main loop, protocol handler)
4. ✅ Port all pin modes (40+ files)
5. ✅ Create ESP-IDF/PlatformIO build configuration
6. ✅ Write installation and usage documentation
7. ✅ Test with Arduino library
8. ✅ Create final validation report

---

## Contact / Questions

If you have questions about the roadmap or considerations document:
- Add comments to the PR
- Edit PORTING_CONSIDERATIONS.md with your questions
- Respond in this conversation

**Ready to proceed?** Let me know your answers to the critical questions (especially Q1, Q4, Q6, Q8, Q14, Q22-Q24) and I'll start the port!

---

*Last Updated: 2026-01-27*  
*Status: Awaiting user review and feedback*
