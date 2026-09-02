#include "hal/BatteryMonitor.h"

#include <Wire.h>
#include <XPowersLib.h>

namespace bowls {

bool BatteryMonitor::begin(TwoWire& wire, int sdaPin, int sclPin) {
    auto* pmu = new XPowersAXP2101();
    if (!pmu->init(wire, sdaPin, sclPin)) {
        delete pmu;
        pmu_ = nullptr;
        available_ = false;
        return false;
    }

    pmu->enableBattDetection();
    pmu->enableBattVoltageMeasure();

    pmu_ = pmu;
    available_ = true;
    return true;
}

int BatteryMonitor::readPercent() {
    if (!available_) return -1;
    int percent = static_cast<XPowersAXP2101*>(pmu_)->getBatteryPercent();
    if (percent < 0 || percent > 100) return -1;
    return percent;
}

}  // namespace bowls
