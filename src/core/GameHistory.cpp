#include "core/GameHistory.h"

namespace bowls {

void GameHistory::addGame(BowlsGame game, uint32_t endTimestamp) {
    if (!game.isFinished()) {
        game.finish(endTimestamp);
    }
    games_.insert(games_.begin(), std::move(game));
}

}  // namespace bowls
