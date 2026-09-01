#pragma once

#include <cstddef>
#include <vector>

#include "core/BowlsGame.h"

namespace bowls {

// Keeps track of every game that has been played so it can be reviewed
// later from the "View old scores" menu option.
class GameHistory {
public:
    // Adds a finished game to the history. The game does not need to be
    // marked finished already; addGame() will call finish() on it if it
    // has not been finished yet.
    void addGame(BowlsGame game, uint32_t endTimestamp = 0);

    size_t count() const { return games_.size(); }
    bool empty() const { return games_.empty(); }

    // Games are stored most-recent-first.
    const BowlsGame& at(size_t index) const { return games_.at(index); }

    const std::vector<BowlsGame>& games() const { return games_; }

    void clear() { games_.clear(); }

private:
    std::vector<BowlsGame> games_;
};

}  // namespace bowls
