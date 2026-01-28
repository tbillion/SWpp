# Serial Wombat ESP32-S3-N16R8 Port Roadmap

**Project Goal**: Port Serial Wombat 18AB firmware to ESP32-S3-N16R8 with 1:1 parity

**Date Created**: 2026-01-27  
**Status**: PLANNING PHASE - DO NOT MODIFY THIS FILE  
**Purpose**: Single source of truth for the porting process

---

## PROJECT OVERVIEW

### Source Firmware
- **Base**: SerialWombat18A_18B (PIC24FJ256GA702)
- **Features**: All 40+ pin modes in unified build
- **Architecture**: Foreground/background loop with 1ms tick
- **Communication**: UART (115200 baud) and I2C slave (0x6B)
- **Protocol**: 8-byte packet binary/ASCII command structure

### Target Hardware
- **Chip**: ESP32-S3-N16R8 (Dual-core Xtensa LX7)
- **Flash**: 16MB
- **PSRAM**: 8MB
- **SRAM**: 512KB (vs 8KB on PIC24)
- **GPIO**: 45 pins available (vs 20 on PIC24)
- **Build System**: ESP-IDF 5.x with PlatformIO support

### Key Differences
- **Memory**: 64x more RAM - no memory constraints
- **Flash**: 64x more flash - all features can be enabled
- **RTOS**: FreeRTOS-based (vs bare-metal loop)
- **DMA**: GDMA channels (vs PIC24 DMA)
- **Timing**: High-resolution timers (vs TMR1/TMR2)

---

## PHASE 1: PREPARATION AND SETUP

### 1.1 Documentation Creation
- [x] Create this roadmap document
- [ ] Create PORTING_CONSIDERATIONS.md (user review items)
- [ ] Document ESP32-S3 pin mapping strategy
- [ ] Document memory layout for ESP32 architecture

### 1.2 Repository Structure
```
SerialWombatESP32S3/
├── README.md                    # Port-specific documentation
├── INSTALLATION.md              # Build and flash instructions
├── platformio.ini               # PlatformIO configuration
├── CMakeLists.txt               # ESP-IDF build configuration
├── sdkconfig.defaults           # ESP-IDF default settings
├── partitions.csv               # Flash partition table
│
├── components/                  # ESP-IDF components
│   └── serial_wombat/
│       ├── CMakeLists.txt
│       ├── include/             # Public headers
│       └── src/                 # Component source
│
├── main/                        # Main application
│   ├── CMakeLists.txt
│   ├── main.c                   # Entry point
│   ├── hw_abstraction/          # Hardware abstraction layer
│   │   ├── esp32_gpio.c/h       # GPIO control
│   │   ├── esp32_timers.c/h     # Timer management
│   │   ├── esp32_dma.c/h        # DMA/high-speed I/O
│   │   ├── esp32_uart.c/h       # UART communication
│   │   ├── esp32_i2c.c/h        # I2C slave interface
│   │   ├── esp32_adc.c/h        # ADC management
│   │   └── esp32_system.c/h     # System init/reset
│   │
│   ├── core/                    # Core firmware (from SW18AB)
│   │   ├── main_loop.c          # Foreground/background loop
│   │   ├── protocol.c           # Command processing (minimal changes)
│   │   ├── pinRegisters.c       # Pin state management (no changes)
│   │   └── utilities.c          # Utility functions
│   │
│   ├── pin_modes/               # All 40+ pin modes
│   │   ├── digitalIO.c
│   │   ├── analogInput.c
│   │   ├── servo.c
│   │   ├── pwm.c
│   │   ├── quadEnc.c
│   │   └── [... all other modes]
│   │
│   └── common/                  # Linked from SerialWombatCommon
│       ├── serialWombat.h       # (symlink)
│       ├── pinModes.h           # (symlink)
│       ├── pinRegisters.h       # (symlink)
│       └── protocol.h           # (symlink)
│
└── docs/
    ├── ARCHITECTURE.md          # ESP32 port architecture
    ├── PROTOCOL.md              # Serial Wombat protocol guide
    ├── PIN_MAPPING.md           # GPIO pin assignments
    └── TROUBLESHOOTING.md       # Common issues and fixes
```

---

## PHASE 2: HARDWARE ABSTRACTION LAYER

### 2.1 System Initialization (esp32_system.c/h)
- [ ] ESP-IDF initialization wrapper
- [ ] FreeRTOS task creation
- [ ] Watchdog configuration
- [ ] Reset handling
- [ ] Flash/NVS initialization
- [ ] System diagnostics

### 2.2 GPIO Abstraction (esp32_gpio.c/h)
- [ ] Pin mapping table (ESP32 GPIO → SW pin numbers)
- [ ] `PinHigh()`, `PinLow()`, `ReadPin()` implementations
- [ ] `PinInput()`, `PinOutput()` direction control
- [ ] Pull-up/pull-down configuration
- [ ] Open-drain support
- [ ] Interrupt attachment for pin modes

**Functions to implement:**
```c
void ESP32_GPIO_Init(void);
void ESP32_PinMode(uint8_t pin, uint8_t mode);
void ESP32_PinHigh(uint8_t pin);
void ESP32_PinLow(uint8_t pin);
bool ESP32_PinRead(uint8_t pin);
void ESP32_PinPullUp(uint8_t pin, bool enable);
void ESP32_PinOpenDrain(uint8_t pin, bool enable);
```

### 2.3 Timer Management (esp32_timers.c/h)
- [ ] 1ms foreground tick (Timer Group 0)
- [ ] 57.6kHz DMA clock equivalent (Timer Group 1 or LEDC)
- [ ] High-resolution timing for pin modes
- [ ] Frame timing diagnostics

**Key timers:**
```c
// Timer 0: 1ms tick for foreground processing
void ESP32_Timer_1ms_Init(void (*callback)(void));

// Timer 1: High-speed sampling (57.6kHz equivalent)
void ESP32_Timer_HighSpeed_Init(uint32_t frequency_hz);

// Microsecond delay
void ESP32_DelayUs(uint32_t us);
```

### 2.4 DMA / High-Speed I/O (esp32_dma.c/h)
- [ ] Circular buffer implementation
- [ ] GPIO register snapshot mechanism
- [ ] High-speed parallel I/O emulation
- [ ] DMA array indexing

**DMA equivalents (may use software emulation or I2S/SPI DMA):**
```c
void ESP32_DMA_Init(void);
void ESP32_DMA_StartSampling(void);
uint16_t* ESP32_DMA_GetInputBuffer(uint8_t port);
uint16_t* ESP32_DMA_GetOutputBuffer(uint8_t port);
uint16_t ESP32_DMA_GetCurrentIndex(uint8_t channel);
```

### 2.5 UART Communication (esp32_uart.c/h)
- [ ] UART0 initialization (115200 baud, 8N1)
- [ ] UART1 optional second interface
- [ ] TX/RX buffering with DMA
- [ ] Packet framing and synchronization

**UART interface:**
```c
void ESP32_UART_Init(uint32_t baud_rate);
void ESP32_UART_WriteBuffer(uint8_t* data, size_t len);
size_t ESP32_UART_ReadBuffer(uint8_t* data, size_t max_len);
bool ESP32_UART_DataAvailable(void);
```

### 2.6 I2C Slave Interface (esp32_i2c.c/h)
- [ ] I2C slave mode (address 0x6B)
- [ ] 8-byte packet read/write
- [ ] Clock stretching support
- [ ] Interrupt-driven operation

**I2C slave:**
```c
void ESP32_I2C_Slave_Init(uint8_t address);
void ESP32_I2C_Slave_SetTxBuffer(uint8_t* data, size_t len);
void ESP32_I2C_Slave_GetRxBuffer(uint8_t* data, size_t* len);
```

### 2.7 ADC Management (esp32_adc.c/h)
- [ ] ADC1/ADC2 initialization
- [ ] Multi-channel scanning
- [ ] DMA-based continuous conversion
- [ ] 16-bit scaled output (0-65535)

**ADC interface:**
```c
void ESP32_ADC_Init(void);
void ESP32_ADC_ConfigurePin(uint8_t pin);
uint16_t ESP32_ADC_Read(uint8_t pin);  // Scaled to 0-65535
```

---

## PHASE 3: CORE FIRMWARE PORT

### 3.1 Main Loop Adaptation (main_loop.c)
- [ ] Port main.c foreground/background structure
- [ ] FreeRTOS task for foreground processing
- [ ] `RunForeground` flag or semaphore-based gating
- [ ] Frame timing and overflow detection
- [ ] Integration with ESP32 timers

**Key changes:**
```c
// Original PIC24 structure:
while (1) {
    ProcessRx();
    if (RunForeground) {
        RunForeground = false;
        ProcessPins();
    }
}

// ESP32 FreeRTOS structure (option 1):
void foreground_task(void* arg) {
    while (1) {
        xSemaphoreTake(foreground_semaphore, portMAX_DELAY);
        ProcessPins();
    }
}

// Timer ISR:
void IRAM_ATTR timer_isr(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(foreground_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### 3.2 Protocol Processing (protocol.c)
- [ ] Copy protocol.c from SW18AB (minimal changes)
- [ ] Adapt UART/I2C backend calls to ESP32 HAL
- [ ] Maintain 8-byte packet structure
- [ ] Binary and ASCII protocol support
- [ ] Command capture to NVS (vs Flash)

**Changes required:**
- Replace `UART1_WriteBuffer()` with `ESP32_UART_WriteBuffer()`
- Replace I2C slave callbacks with ESP32 equivalents
- Flash read/write → ESP32 NVS or partition API

### 3.3 Pin Register Management (pinRegisters.c)
- [ ] Direct copy from SW18AB (no changes needed)
- [ ] 96-byte pin register structure preserved
- [ ] 40 pin allocation (30 physical + 10 virtual)
- [ ] Public data access functions

**No changes required** - this is platform-agnostic.

### 3.4 Utilities (utilities.c)
- [ ] Port utility functions
- [ ] CRC calculations (use ESP32 hardware CRC)
- [ ] Math functions (linear interpolation)
- [ ] ASCII conversion functions

---

## PHASE 4: PIN MODES PORTING

**Strategy**: Port all 40+ pin modes from SW18AB to maintain 1:1 parity.

### 4.1 Basic Pin Modes (Priority 1)
- [ ] PIN_MODE_DIGITAL_IO (digitalIO.c)
- [ ] PIN_MODE_ANALOGINPUT (analogInput.c)
- [ ] PIN_MODE_PWM (pwm.c)
- [ ] PIN_MODE_SERVO (servo.c)
- [ ] PIN_MODE_DEBOUNCE (debounce.c)

### 4.2 Communication Pin Modes (Priority 2)
- [ ] PIN_MODE_UART0_TXRX (uartHw.c)
- [ ] PIN_MODE_UART1_TXRX (uartHw.c)
- [ ] PIN_MODE_SW_UART (uartSw.c)
- [ ] PIN_MODE_I2C_CONTROLLER (i2cController.c)

### 4.3 Input Pin Modes (Priority 2)
- [ ] PIN_MODE_COUNTER (counter.c)
- [ ] PIN_MODE_QUADRATURE_ENC (quadEnc.c)
- [ ] PIN_MODE_PULSE_TIMER (pulseTimer.c)
- [ ] PIN_MODE_PULSE_ON_CHANGE (pulseOnChange.c)
- [ ] PIN_MODE_TOUCH (touch.c)
- [ ] PIN_MODE_RESISTANCE_INPUT (resistanceInput.c)
- [ ] PIN_MODE_ULTRASONIC_DISTANCE (ultrasonicDistance.c)

### 4.4 Output Pin Modes (Priority 2)
- [ ] PIN_MODE_PROTECTEDOUTPUT (protectedOutput.c)
- [ ] PIN_MODE_HBRIDGE (hBridge.c)
- [ ] PIN_MODE_HF_SERVO (hfServo.c)
- [ ] PIN_MODE_DMA_PULSE_OUTPUT (pulseInputDMA.c)
- [ ] PIN_MODE_QUEUED_PULSE_OUTPUT (queuedPulseOutput.c)
- [ ] PIN_MODE_FREQUENCY_OUTPUT (frequencyOutput.c)

### 4.5 Advanced Pin Modes (Priority 3)
- [ ] PIN_MODE_WS2812 (ws2812.c)
- [ ] PIN_MODE_TM1637 (TM1637.c)
- [ ] PIN_MODE_MATRIX_KEYPAD (matrixKeypad.c)
- [ ] PIN_MODE_LIQUID_CRYSTAL (liquidCrystal.c)
- [ ] PIN_MODE_PS2_KEYBOARD (PS2Keyboard.c)
- [ ] PIN_MODE_MAX7219MATRIX (MAX7219Matrix.c)
- [ ] PIN_MODE_VGA (vga.c)
- [ ] PIN_MODE_HS_CLOCK (HSClock.c)
- [ ] PIN_MODE_HS_COUNTER (HSCounter.c)

### 4.6 Processing Pin Modes (Priority 2)
- [ ] PIN_MODE_INPUT_PROCESSOR (inputProcess.c)
- [ ] PIN_MODE_WATCHDOG (watchdog.c)
- [ ] PIN_MODE_THROUGHPUT_CONSUMER (throughputConsumer.c)
- [ ] PIN_MODE_FRAME_TIMER (pinInputProcessor.c)

### 4.7 Pin Mode Porting Checklist (per mode)
Each pin mode must:
1. Copy source file from SW18AB
2. Replace hardware-specific calls with ESP32 HAL
3. Test init function (configuration)
4. Test update function (1ms processing)
5. Verify public data access
6. Test with protocol commands
7. Validate timing requirements

---

## PHASE 5: BUILD SYSTEM INTEGRATION

### 5.1 ESP-IDF Configuration
- [ ] Create CMakeLists.txt (root)
- [ ] Create main/CMakeLists.txt
- [ ] Create sdkconfig.defaults
- [ ] Configure partitions.csv
- [ ] Set compiler flags (optimization, warnings)

**sdkconfig.defaults settings:**
```ini
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_TIMER_IMPL_SYSTIMER=y
```

### 5.2 PlatformIO Configuration
- [ ] Create platformio.ini
- [ ] Configure ESP-IDF platform
- [ ] Set board = esp32-s3-devkitc-1 (or custom)
- [ ] Configure upload/monitor settings

**platformio.ini:**
```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = espidf
board_build.flash_mode = qio
board_build.psram_type = opi
board_build.memory_type = qio_opi
monitor_speed = 115200
```

### 5.3 Build Verification
- [ ] Build without errors
- [ ] Flash to ESP32-S3
- [ ] Verify boot messages
- [ ] Test basic communication

---

## PHASE 6: TESTING AND VALIDATION

### 6.1 Communication Testing
- [ ] UART packet send/receive
- [ ] I2C slave read/write
- [ ] Protocol echo test
- [ ] ASCII command parsing
- [ ] Binary command parsing

### 6.2 Pin Mode Testing (per mode)
- [ ] Configuration commands
- [ ] Public data read/write
- [ ] State machine execution
- [ ] Timing accuracy
- [ ] Multiple simultaneous pins

### 6.3 Performance Testing
- [ ] Frame timing (1ms accuracy)
- [ ] Frame overflow detection
- [ ] CPU utilization measurement
- [ ] Memory usage analysis
- [ ] DMA buffer integrity

### 6.4 Protocol Compliance Testing
- [ ] Test with Arduino library
- [ ] Test with Python library
- [ ] Verify all protocol commands
- [ ] Error handling validation

---

## PHASE 7: DOCUMENTATION

### 7.1 Installation Guide (INSTALLATION.md)
- [ ] Prerequisites (ESP-IDF, PlatformIO)
- [ ] Clone and build instructions
- [ ] Flashing instructions
- [ ] Initial configuration steps
- [ ] Troubleshooting common issues

### 7.2 Architecture Documentation (ARCHITECTURE.md)
- [ ] ESP32 port design decisions
- [ ] Hardware abstraction layer overview
- [ ] Memory layout and usage
- [ ] Timing and interrupt structure
- [ ] DMA implementation details

### 7.3 Pin Mapping Guide (PIN_MAPPING.md)
- [ ] ESP32-S3 GPIO to SW pin mapping table
- [ ] ADC-capable pins
- [ ] Special function pins (UART, I2C)
- [ ] Reserved pins
- [ ] Pin constraints and limitations

### 7.4 Usage Guide (README.md)
- [ ] Quick start guide
- [ ] Communication setup (UART/I2C)
- [ ] Example configurations
- [ ] Pin mode usage examples
- [ ] Integration with host libraries

### 7.5 Protocol Documentation (PROTOCOL.md)
- [ ] Link to Serial Wombat protocol spec
- [ ] ESP32-specific considerations
- [ ] Performance characteristics
- [ ] Known limitations vs PIC24 version

---

## PHASE 8: FINAL VALIDATION

### 8.1 Feature Parity Verification
- [ ] All 40+ pin modes functional
- [ ] All protocol commands working
- [ ] Timing accuracy validated
- [ ] Memory safety confirmed

### 8.2 Code Quality
- [ ] No compiler warnings
- [ ] Memory leaks checked (FreeRTOS heap)
- [ ] Static analysis passed
- [ ] Code review completed

### 8.3 Release Preparation
- [ ] Version numbering scheme
- [ ] Release notes
- [ ] Binary builds (if applicable)
- [ ] GitHub release creation

---

## SUCCESS CRITERIA

### Must Have (1:1 Parity)
✅ All 40+ pin modes from SW18AB functional  
✅ Serial Wombat protocol fully compliant  
✅ UART and I2C communication working  
✅ 1ms foreground timing maintained  
✅ Compatible with existing Arduino/Python libraries  
✅ No memory constraints (all features enabled)  

### Should Have
✅ Comprehensive documentation  
✅ ESP-IDF and PlatformIO support  
✅ Performance equal or better than PIC24 version  
✅ Easy installation process  

### Nice to Have
⭐ Additional ESP32-specific features (WiFi, BLE)  
⭐ OTA firmware updates  
⭐ Web-based configuration  
⭐ Enhanced diagnostics  

---

## RISK MITIGATION

### Technical Risks
- **DMA implementation complexity**: May require software emulation
  - *Mitigation*: Use timer-based circular buffer updates
  
- **Timing accuracy**: FreeRTOS vs bare-metal
  - *Mitigation*: Use high-priority tasks and timer groups
  
- **I2C slave reliability**: ESP32 I2C slave can be problematic
  - *Mitigation*: Extensive testing, consider bit-bang fallback

### Process Risks
- **Scope creep**: Adding ESP32-specific features
  - *Mitigation*: Strict 1:1 parity requirement first
  
- **Testing coverage**: 40+ pin modes to validate
  - *Mitigation*: Automated testing framework, Arduino library tests

---

## TIMELINE ESTIMATE

**Total Effort**: ~80-120 hours

- Phase 1 (Preparation): 4-8 hours
- Phase 2 (HAL): 16-24 hours
- Phase 3 (Core): 8-12 hours
- Phase 4 (Pin Modes): 32-48 hours (longest phase)
- Phase 5 (Build System): 4-6 hours
- Phase 6 (Testing): 8-12 hours
- Phase 7 (Documentation): 6-8 hours
- Phase 8 (Validation): 2-4 hours

---

## CURRENT STATUS

**Last Updated**: 2026-01-27  
**Current Phase**: PHASE 1 - Preparation  
**Next Steps**: Create PORTING_CONSIDERATIONS.md for user review

---

**IMPORTANT NOTES:**
1. This roadmap should NOT be modified during development
2. If lost during development, refer to this document for direction
3. Check off items as completed but do not reorder or add sections
4. Any deviations from plan should be documented separately
5. User review of PORTING_CONSIDERATIONS.md required before Phase 2

---

*END OF ROADMAP*
