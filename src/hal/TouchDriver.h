#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace bowls {

// Minimal polling driver for the FT3168 capacitive touch controller used on
// the Waveshare ESP32-S3-Touch-AMOLED-1.8.
class TouchDriver {
public:
    TouchDriver(int sdaPin, int sclPin, int intPin, uint8_t i2cAddress = 0x38);

    void begin();

    // Polls the controller for the current touch state. Returns true if a
    // finger is currently pressed, and fills x/y with panel coordinates.
    bool read(int16_t& x, int16_t& y);

private:
    static void onInterrupt();

    int sdaPin_;
    int sclPin_;
    int intPin_;
    uint8_t i2cAddress_;

    static volatile bool s_touchPending_;
};

}  // namespace bowls
