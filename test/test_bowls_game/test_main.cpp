#include <unity.h>

#include "core/BowlsGame.h"
#include "core/GameHistory.h"

using namespace bowls;

void setUp(void) {}
void tearDown(void) {}

static void test_singles_max_per_end(void) {
    BowlsGame g(GameType::Singles, {"Alice"}, {"Bob"}, 100);
    TEST_ASSERT_EQUAL_INT(2, g.maxPerEnd());
    TEST_ASSERT_TRUE(g.recordEnd(2, 0));
    TEST_ASSERT_FALSE(g.recordEnd(3, 0));  // exceeds max
    TEST_ASSERT_FALSE(g.recordEnd(1, 1));  // both sides can't score
    TEST_ASSERT_FALSE(g.recordEnd(0, 0));  // no winner
}

static void test_doubles_max_per_end(void) {
    BowlsGame g(GameType::Doubles, {"Alice", "Ann"}, {"Bob", "Bill"});
    TEST_ASSERT_EQUAL_INT(4, g.maxPerEnd());
    TEST_ASSERT_TRUE(g.recordEnd(4, 0));
    TEST_ASSERT_FALSE(g.recordEnd(5, 0));
    TEST_ASSERT_TRUE(g.recordEnd(0, 3));
    TEST_ASSERT_EQUAL_INT(4, g.team1().score);
    TEST_ASSERT_EQUAL_INT(3, g.team2().score);
}

static void test_undo_last_end(void) {
    BowlsGame g(GameType::Singles, {"Alice"}, {"Bob"});
    g.recordEnd(2, 0);
    g.recordEnd(0, 1);
    TEST_ASSERT_TRUE(g.undoLastEnd());
    TEST_ASSERT_EQUAL_INT(0, g.team2().score);
    TEST_ASSERT_EQUAL_INT(1, g.endCount());
}

static void test_finish_prevents_further_scoring(void) {
    BowlsGame g(GameType::Singles, {"Alice"}, {"Bob"});
    g.recordEnd(2, 0);
    g.finish(200);
    TEST_ASSERT_TRUE(g.isFinished());
    TEST_ASSERT_FALSE(g.recordEnd(1, 0));
    TEST_ASSERT_EQUAL_STRING("Team 1 won", g.resultSummary().c_str());
}

static void test_dead_end_scores_nothing(void) {
    BowlsGame g(GameType::Singles, {"Alice"}, {"Bob"});
    g.recordEnd(1, 0);
    TEST_ASSERT_TRUE(g.recordDeadEnd());
    TEST_ASSERT_EQUAL_INT(1, g.team1().score);
    TEST_ASSERT_EQUAL_INT(0, g.team2().score);
    TEST_ASSERT_EQUAL_INT(2, g.endCount());
    TEST_ASSERT_TRUE(g.undoLastEnd());
    TEST_ASSERT_EQUAL_INT(1, g.endCount());
}

static void test_handicaps_and_winning_score(void) {
    BowlsGame g(GameType::Singles, {"Alice"}, {"Bob"}, 0, 11, 3, 1);
    TEST_ASSERT_EQUAL_INT(11, g.winningScore());
    TEST_ASSERT_EQUAL_INT(3, g.team1().score);
    TEST_ASSERT_EQUAL_INT(1, g.team2().score);
    TEST_ASSERT_FALSE(g.hasReachedWinningScore());
    TEST_ASSERT_TRUE(g.recordEnd(2, 0));
    TEST_ASSERT_TRUE(g.recordEnd(2, 0));
    TEST_ASSERT_TRUE(g.recordEnd(2, 0));
    TEST_ASSERT_TRUE(g.recordEnd(2, 0));
    TEST_ASSERT_TRUE(g.hasReachedWinningScore());
}

static void test_game_history_orders_most_recent_first(void) {
    GameHistory history;
    BowlsGame g1(GameType::Singles, {"A"}, {"B"}, 1);
    g1.recordEnd(2, 0);
    history.addGame(g1, 10);

    BowlsGame g2(GameType::Singles, {"C"}, {"D"}, 2);
    g2.recordEnd(1, 0);
    history.addGame(g2, 20);

    TEST_ASSERT_EQUAL_UINT(2, history.count());
    TEST_ASSERT_EQUAL_STRING("C", history.at(0).team1().playerNames[0].c_str());
    TEST_ASSERT_EQUAL_STRING("A", history.at(1).team1().playerNames[0].c_str());
    TEST_ASSERT_TRUE(history.at(0).isFinished());
    TEST_ASSERT_EQUAL_UINT32(20, history.at(0).endTimestamp());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_singles_max_per_end);
    RUN_TEST(test_doubles_max_per_end);
    RUN_TEST(test_undo_last_end);
    RUN_TEST(test_finish_prevents_further_scoring);
    RUN_TEST(test_dead_end_scores_nothing);
    RUN_TEST(test_handicaps_and_winning_score);
    RUN_TEST(test_game_history_orders_most_recent_first);
    return UNITY_END();
}
