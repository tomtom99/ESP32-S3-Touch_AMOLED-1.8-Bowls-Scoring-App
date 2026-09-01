#include "core/BowlsGame.h"

namespace bowls {

int playersPerSide(GameType type) {
    return type == GameType::Doubles ? 2 : 1;
}

int maxScorePerEnd(GameType type) {
    return playersPerSide(type) * 2;
}

BowlsGame::BowlsGame(GameType type,
                      std::vector<std::string> team1Names,
                      std::vector<std::string> team2Names,
                      uint32_t startTimestamp)
    : type_(type), startTimestamp_(startTimestamp) {
    team1_.playerNames = std::move(team1Names);
    team2_.playerNames = std::move(team2Names);
}

bool BowlsGame::recordEnd(int team1Score, int team2Score) {
    if (finished_) {
        return false;
    }
    if (team1Score < 0 || team2Score < 0) {
        return false;
    }
    // Only one side can win an end; the other side scores zero.
    if (team1Score > 0 && team2Score > 0) {
        return false;
    }
    if (team1Score == 0 && team2Score == 0) {
        return false;
    }
    const int maxPerEnd = maxScorePerEnd(type_);
    if (team1Score > maxPerEnd || team2Score > maxPerEnd) {
        return false;
    }

    EndResult endResult;
    endResult.team1Score = team1Score;
    endResult.team2Score = team2Score;
    ends_.push_back(endResult);
    team1_.score += team1Score;
    team2_.score += team2Score;
    return true;
}

bool BowlsGame::undoLastEnd() {
    if (finished_ || ends_.empty()) {
        return false;
    }
    const EndResult& last = ends_.back();
    team1_.score -= last.team1Score;
    team2_.score -= last.team2Score;
    ends_.pop_back();
    return true;
}

void BowlsGame::finish(uint32_t endTimestamp) {
    finished_ = true;
    endTimestamp_ = endTimestamp;
}

std::string BowlsGame::resultSummary() const {
    if (team1_.score > team2_.score) {
        return "Team 1 won";
    }
    if (team2_.score > team1_.score) {
        return "Team 2 won";
    }
    return "Draw";
}

}  // namespace bowls
