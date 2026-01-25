#include "RGBLed.h"
#include <math.h>

RGBLed::RGBLed() 
    : currentStatus(LED_WAITING), 
      brightness(255), 
      breathPhase(0.0f), 
      breathSpeed(0.05f),
      breathing(false) {
}

void RGBLed::begin() {
    pinMode(RGB_LED_RED_PIN, OUTPUT);
    pinMode(RGB_LED_GREEN_PIN, OUTPUT);
    pinMode(RGB_LED_BLUE_PIN, OUTPUT);
    
    ledcSetup(LED_PWM_RED_CHANNEL, LED_PWM_FREQ, LED_PWM_RESOLUTION);
    ledcSetup(LED_PWM_GREEN_CHANNEL, LED_PWM_FREQ, LED_PWM_RESOLUTION);
    ledcSetup(LED_PWM_BLUE_CHANNEL, LED_PWM_FREQ, LED_PWM_RESOLUTION);
    
    ledcAttachPin(RGB_LED_RED_PIN, LED_PWM_RED_CHANNEL);
    ledcAttachPin(RGB_LED_GREEN_PIN, LED_PWM_GREEN_CHANNEL);
    ledcAttachPin(RGB_LED_BLUE_PIN, LED_PWM_BLUE_CHANNEL);
    
    setStatus(LED_WAITING);
}

void RGBLed::setStatus(LEDStatus status) {
    currentStatus = status;
    breathing = false;
    breathPhase = 0.0f;
    
    switch(status) {
        case LED_WAITING:
            setColor(0, brightness, 0);
            break;
        case LED_LOADING:
            setColor(brightness, brightness, 0);
            break;
        case LED_BUSY:
            setColor(brightness, 0, 0);
            breathing = true;
            break;
    }
}

void RGBLed::update() {
    if (breathing) {
        breathPhase += breathSpeed;
        if (breathPhase > TWO_PI) {
            breathPhase -= TWO_PI;
        }
        
        switch(currentStatus) {
            case LED_BUSY:
                setColor(calculateBreathValue(brightness), 0, 0);
                break;
            default:
                break;
        }
    }
}

void RGBLed::setBrightness(uint8_t newBrightness) {
    brightness = newBrightness;
    setStatus(currentStatus);
}

void RGBLed::setColor(uint8_t red, uint8_t green, uint8_t blue) {
    ledcWrite(LED_PWM_RED_CHANNEL, red);
    ledcWrite(LED_PWM_GREEN_CHANNEL, green);
    ledcWrite(LED_PWM_BLUE_CHANNEL, blue);
}

uint8_t RGBLed::calculateBreathValue(uint8_t baseValue) {
    float sinValue = (sin(breathPhase) + 1.0f) / 2.0f;
    uint8_t minValue = baseValue / 4;
    return minValue + (uint8_t)((baseValue - minValue) * sinValue);
}
