#include "hal/TouchDriver.h"

namespace bowls {

TouchDriver::TouchDriver(int sdaPin, int sclPin, int intPin, uint8_t i2cAddress)
    : sdaPin_(sdaPin), sclPin_(sclPin), intPin_(intPin), i2cAddress_(i2cAddress) {}

void TouchDriver::begin() {
    pinMode(intPin_, INPUT);
    Wire.begin(sdaPin_, sclPin_);
}

bool TouchDriver::read(int16_t& x, int16_t& y) {
    // CST820 register map: 0x02 = number of touch points, 0x03-0x04 = X
    // (high nibble of 0x03 holds the event flag), 0x05-0x06 = Y.
    Wire.beginTransmission(i2cAddress_);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(static_cast<int>(i2cAddress_), 5) != 5) {
        return false;
    }

    const uint8_t fingers = Wire.read();
    const uint8_t xh = Wire.read();
    const uint8_t xl = Wire.read();
    const uint8_t yh = Wire.read();
    const uint8_t yl = Wire.read();

    if (fingers == 0) {
        return false;
    }

    x = static_cast<int16_t>(((xh & 0x0F) << 8) | xl);
    y = static_cast<int16_t>(((yh & 0x0F) << 8) | yl);
    return true;
}

}  // namespace bowls
