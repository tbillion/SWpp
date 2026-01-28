# Serial Wombat ESP32-S3 Main Component
# Hardware Abstraction Layer + Core Firmware

set(COMPONENT_SRCS
    "main.c"
    "hw_abstraction/esp32_gpio.c"
    "hw_abstraction/esp32_timers.c"
    "hw_abstraction/esp32_uart.c"
    "hw_abstraction/esp32_i2c.c"
    "hw_abstraction/esp32_adc.c"
    "hw_abstraction/esp32_dma.c"
    "hw_abstraction/esp32_system.c"
    # Phase 3: Core Firmware
    "core/protocol.c"
    "core/pinRegisters.c"
)

set(COMPONENT_ADD_INCLUDEDIRS
    "."
    "hw_abstraction"
    "core"
    "pin_modes"
    "common"
)

set(COMPONENT_REQUIRES
    driver
    esp_timer
    freertos
    nvs_flash
)

register_component()

# Component will be built when sources are added
