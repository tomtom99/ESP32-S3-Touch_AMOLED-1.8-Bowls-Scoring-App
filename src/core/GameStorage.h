#pragma once

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
};

}  // namespace bowls
