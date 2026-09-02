#pragma once

#include <Arduino.h>

#include "core/GameStorage.h"

namespace bowls {

// Persists game history as a JSON file on the board's LittleFS flash
// filesystem, so past games survive a reboot or power cycle.
class FlashGameStorage : public GameStorage {
public:
    explicit FlashGameStorage(const char* path = "/history.json");

    // Mounts (and if necessary formats) the LittleFS filesystem. Must be
    // called once before load()/save() are used.
    bool begin();

    bool load(GameHistory& history) override;
    bool save(const GameHistory& history) override;
    bool loadInProgress(BowlsGame& game) override;
    bool saveInProgress(const BowlsGame& game) override;
    bool clearInProgress() override;

private:
    const char* path_;
    const char* inProgressPath_ = "/current-game.json";
};

}  // namespace bowls
