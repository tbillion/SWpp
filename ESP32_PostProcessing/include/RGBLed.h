#ifndef RGBLED_H
#define RGBLED_H

#include <Arduino.h>
#include "config.h"

enum LEDStatus {
    LED_WAITING,    // Green - waiting for user input
    LED_LOADING,    // Yellow - loading file
    LED_BUSY        // Red - processing
};

class RGBLed {
public:
    RGBLed();
    void begin();
    void setStatus(LEDStatus status);
    void update();
    void setBrightness(uint8_t brightness);
    
private:
    LEDStatus currentStatus;
    uint8_t brightness;
    float breathPhase;
    float breathSpeed;
    bool breathing;
    
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
    uint8_t calculateBreathValue(uint8_t baseValue);
};

#endif // RGBLED_H
