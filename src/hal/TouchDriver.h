#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace bowls {

// Minimal polling driver for the CST820 capacitive touch controller used on
// the Waveshare ESP32-S3-Touch-AMOLED-1.8 (V2 boards). Older V1 boards use
// an FT3168 controller instead; if you have a V1 board, swap this driver
// for one that speaks the FT3168 register map.
class TouchDriver {
public:
    TouchDriver(int sdaPin, int sclPin, int intPin, uint8_t i2cAddress = 0x15);

    void begin();

    // Polls the controller for the current touch state. Returns true if a
    // finger is currently pressed, and fills x/y with panel coordinates.
    bool read(int16_t& x, int16_t& y);

private:
    int sdaPin_;
    int sclPin_;
    int intPin_;
    uint8_t i2cAddress_;
};

}  // namespace bowls
