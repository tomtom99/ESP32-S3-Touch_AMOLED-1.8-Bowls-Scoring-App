#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bowls {

// A game is either played as singles (1 player per side) or doubles
// (2 players per side). Each player has 2 bowls, so the maximum number
// of points a side can score in a single end is 2 * playersPerSide.
enum class GameType : uint8_t {
    Singles = 0,
    Doubles = 1
};

// Returns the number of players on each side for a given game type.
int playersPerSide(GameType type);

// Returns the maximum number of points a side can score in a single end
// for a given game type (2 bowls per player).
int maxScorePerEnd(GameType type);

// A single team taking part in a game (1 player for singles, 2 for doubles).
struct Team {
    std::vector<std::string> playerNames;
    int score = 0;
};

// The result recorded for one end of play. Only one side can win an end in
// crown green bowls, so at most one of the two scores may be non-zero.
struct EndResult {
    int team1Score = 0;
    int team2Score = 0;
};

// Represents a single completed or in-progress game of crown green bowls.
class BowlsGame {
public:
    BowlsGame() = default;

    // Creates a new game. team1Names/team2Names must contain exactly
    // playersPerSide(type) entries.
    BowlsGame(GameType type,
              std::vector<std::string> team1Names,
              std::vector<std::string> team2Names,
              uint32_t startTimestamp = 0);

    GameType type() const { return type_; }
    const Team& team1() const { return team1_; }
    const Team& team2() const { return team2_; }
    int maxPerEnd() const { return maxScorePerEnd(type_); }
    int endCount() const { return static_cast<int>(ends_.size()); }
    const std::vector<EndResult>& ends() const { return ends_; }
    uint32_t startTimestamp() const { return startTimestamp_; }
    uint32_t endTimestamp() const { return endTimestamp_; }
    bool isFinished() const { return finished_; }

    // Records the result of one end. Exactly one of team1Score/team2Score
    // must be zero (a side only scores when it wins the end), and the
    // non-zero value must be between 1 and maxPerEnd() inclusive.
    // Returns false (and records nothing) if the input is invalid or the
    // game has already been finished.
    bool recordEnd(int team1Score, int team2Score);

    // Removes the most recently recorded end, undoing its score. Returns
    // false if there are no ends to undo or the game is finished.
    bool undoLastEnd();

    // Marks the game as finished. After this, recordEnd() will fail.
    void finish(uint32_t endTimestamp = 0);

    // Returns a short human readable description of the winner, e.g.
    // "Team 1 won" / "Team 2 won" / "Draw".
    std::string resultSummary() const;

private:
    GameType type_ = GameType::Singles;
    Team team1_;
    Team team2_;
    std::vector<EndResult> ends_;
    uint32_t startTimestamp_ = 0;
    uint32_t endTimestamp_ = 0;
    bool finished_ = false;
};

}  // namespace bowls
