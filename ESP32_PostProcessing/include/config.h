#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Hardware Configuration for ESP32-32E 3.5" Display
// Reference: https://www.lcdwiki.com/3.5inch_ESP32-32E_Display

// RGB LED Pins (onboard indicator)
#define RGB_LED_RED_PIN     4
#define RGB_LED_GREEN_PIN   16
#define RGB_LED_BLUE_PIN    17

// SD Card SPI Interface
#define SD_MISO_PIN         2
#define SD_MOSI_PIN         15
#define SD_CLK_PIN          14
#define SD_CS_PIN           13

// TFT Display SPI Interface
#define TFT_MOSI_PIN        23
#define TFT_MISO_PIN        19
#define TFT_CLK_PIN         18
#define TFT_CS_PIN          5
#define TFT_RST_PIN         33
#define TFT_DC_PIN          27

// Display specifications
#define TFT_WIDTH           320
#define TFT_HEIGHT          480

// I2C Interface (optional expansion)
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22

// Buzzer and Speaker (optional)
#define BUZZER_PIN          26
#define SPEAKER_PIN         25

// LED PWM Configuration for breathing effect
#define LED_PWM_FREQ        5000
#define LED_PWM_RESOLUTION  8
#define LED_PWM_RED_CHANNEL 0
#define LED_PWM_GREEN_CHANNEL 1
#define LED_PWM_BLUE_CHANNEL 2

// Memory configuration
#define MAX_HEX_MEMORY      0x40000  // 256KB max hex file size in memory

// Processing configuration
#define BASE_ADDRESS        0x8000
#define LENGTH_RANGE        0x38000
#define MAGIC_ADDR          0x1F800
#define CRC_ADDR            0x1F804

#endif // CONFIG_H
