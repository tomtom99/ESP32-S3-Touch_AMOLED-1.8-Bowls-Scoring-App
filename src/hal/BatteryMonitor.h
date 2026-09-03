#pragma once

class TwoWire;

namespace bowls {

// Reads the battery percentage from the onboard AXP2101 power-management
// chip (I2C) via XPowersLib. The board has no separate battery ADC pin;
// all battery data is exposed through this PMU.
class BatteryMonitor {
public:
    // Probes for the AXP2101 on the given (already-begun) I2C bus. Returns
    // false if the chip isn't found/responding.
    bool begin(TwoWire& wire, int sdaPin, int sclPin);

    // Returns the battery percentage (0-100), or -1 if unavailable (chip
    // not found, or no battery connected).
    int readPercent();

private:
    void* pmu_ = nullptr;
    bool available_ = false;
};

}  // namespace bowls
