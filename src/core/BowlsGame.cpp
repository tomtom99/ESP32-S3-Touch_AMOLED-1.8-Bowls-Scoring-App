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
                      uint32_t startTimestamp,
                      int winningScore,
                      int team1Handicap,
                      int team2Handicap)
    : type_(type), startTimestamp_(startTimestamp), winningScore_(winningScore) {
    team1_.playerNames = std::move(team1Names);
    team2_.playerNames = std::move(team2Names);
    team1_.handicap = team1Handicap;
    team2_.handicap = team2Handicap;
    team1_.score = team1Handicap;
    team2_.score = team2Handicap;
}

bool BowlsGame::hasReachedWinningScore() const {
    return team1_.score >= winningScore_ || team2_.score >= winningScore_;
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

bool BowlsGame::recordDeadEnd() {
    if (finished_) {
        return false;
    }
    EndResult endResult;
    endResult.team1Score = 0;
    endResult.team2Score = 0;
    ends_.push_back(endResult);
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

bool BowlsGame::removeEnd(size_t index) {
    if (finished_ || index >= ends_.size()) {
        return false;
    }
    const EndResult& target = ends_[index];
    team1_.score -= target.team1Score;
    team2_.score -= target.team2Score;
    ends_.erase(ends_.begin() + static_cast<std::vector<EndResult>::difference_type>(index));
    return true;
}

bool BowlsGame::editEnd(size_t index, int team1Score, int team2Score) {
    if (finished_ || index >= ends_.size()) {
        return false;
    }
    if (team1Score < 0 || team2Score < 0) {
        return false;
    }
    // Only one side can win an end; the other side scores zero.
    if (team1Score > 0 && team2Score > 0) {
        return false;
    }
    const int maxPerEnd = maxScorePerEnd(type_);
    if (team1Score > maxPerEnd || team2Score > maxPerEnd) {
        return false;
    }

    EndResult& target = ends_[index];
    team1_.score += team1Score - target.team1Score;
    team2_.score += team2Score - target.team2Score;
    target.team1Score = team1Score;
    target.team2Score = team2Score;
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
