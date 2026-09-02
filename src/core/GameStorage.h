#pragma once

#include <cstdint>

#include "core/GameHistory.h"

namespace bowls {

// Abstraction over persistent storage for game history, so the core logic
// and UI do not need to depend directly on the ESP32 filesystem APIs.
class GameStorage {
public:
    virtual ~GameStorage() = default;

    // Loads any previously saved games into the given history object.
    // Existing entries in history are not cleared beforehand.
    virtual bool load(GameHistory& history) = 0;

    // Persists the full contents of history to storage, overwriting
    // whatever was previously saved.
    virtual bool save(const GameHistory& history) = 0;

    // Loads, saves, and clears the single game that has not been completed
    // yet. A false result from loadInProgress() means there is no game to
    // resume or that it could not be read.
    virtual bool loadInProgress(BowlsGame& game) = 0;
    virtual bool saveInProgress(const BowlsGame& game) = 0;
    virtual bool clearInProgress() = 0;

    // Loads/saves the display brightness (0-255). A false result from
    // loadBrightness() means no value has been saved yet.
    virtual bool loadBrightness(uint8_t& brightness) = 0;
    virtual bool saveBrightness(uint8_t brightness) = 0;

    // Wipes all persisted data: history, in-progress game and settings.
    virtual bool resetAll() = 0;
};

}  // namespace bowls
