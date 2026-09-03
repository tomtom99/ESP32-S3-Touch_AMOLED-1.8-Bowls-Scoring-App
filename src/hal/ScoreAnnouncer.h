#pragma once

#include <cstddef>
#include <cstdint>

class TwoWire;

namespace bowls {

class ScoreAnnouncer {
public:
    bool begin(TwoWire& wire, int sdaPin, int sclPin);
    void setVolumePercent(uint8_t volumePercent);
    void announceScore(int homeScore, int awayScore, bool deadEnd);

private:
    void playScore(int score);
    TwoWire* wire_ = nullptr;
    bool initialized_ = false;
};

}  // namespace bowls
