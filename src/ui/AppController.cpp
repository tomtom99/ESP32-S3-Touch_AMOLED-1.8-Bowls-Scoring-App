#include "ui/AppController.h"

#include <cstdio>

namespace bowls {

AppController* AppController::s_instance = nullptr;

AppController::AppController(GameStorage& storage) : storage_(storage) {
    s_instance = this;
}

void AppController::begin() {
    storage_.load(history_);
    showMenu();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

lv_obj_t* AppController::createScreen() {
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_scr_load(screen);
    return screen;
}

lv_obj_t* AppController::addTitle(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    return label;
}

lv_obj_t* AppController::addButton(lv_obj_t* parent, const char* text, lv_event_cb_t callback) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------

void AppController::showMenu() {
    lv_obj_t* screen = createScreen();
    addTitle(screen, "Crown Green Bowls");

    lv_obj_t* newGameBtn = addButton(screen, "New Game", onMenuNewGame);
    lv_obj_align(newGameBtn, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t* historyBtn = addButton(screen, "View Old Scores", onMenuHistory);
    lv_obj_align(historyBtn, LV_ALIGN_CENTER, 0, 30);
}

void AppController::onMenuNewGame(lv_event_t*) {
    s_instance->showNewGameSetup();
}

void AppController::onMenuHistory(lv_event_t*) {
    s_instance->showHistoryList();
}

// ---------------------------------------------------------------------------
// New game setup
// ---------------------------------------------------------------------------

void AppController::rebuildNameInputs(lv_obj_t* container) {
    lv_obj_clean(container);
    const int players = playersPerSide(pendingType_) * 2;  // both teams
    for (int i = 0; i < 4; ++i) {
        nameInputs_[i] = nullptr;
    }
    for (int i = 0; i < players; ++i) {
        lv_obj_t* ta = lv_textarea_create(container);
        lv_textarea_set_one_line(ta, true);
        char placeholder[16];
        std::snprintf(placeholder, sizeof(placeholder), "Player %d", i + 1);
        lv_textarea_set_placeholder_text(ta, placeholder);
        lv_obj_add_event_cb(ta, onTextAreaFocused, LV_EVENT_FOCUSED, nullptr);
        nameInputs_[i] = ta;
    }
}

void AppController::onTextAreaFocused(lv_event_t* e) {
    lv_obj_t* ta = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (s_instance->keyboard_ != nullptr) {
        lv_keyboard_set_textarea(s_instance->keyboard_, ta);
        lv_obj_clear_flag(s_instance->keyboard_, LV_OBJ_FLAG_HIDDEN);
    }
}

void AppController::showNewGameSetup() {
    pendingType_ = GameType::Singles;
    lv_obj_t* screen = createScreen();
    addTitle(screen, "New Game");

    lv_obj_t* typeRow = lv_obj_create(screen);
    lv_obj_set_size(typeRow, LV_PCT(90), 50);
    lv_obj_align(typeRow, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_flex_flow(typeRow, LV_FLEX_FLOW_ROW);

    lv_obj_t* singlesBtn = lv_btn_create(typeRow);
    lv_obj_add_flag(singlesBtn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_state(singlesBtn, LV_STATE_CHECKED);
    lv_obj_add_event_cb(singlesBtn, onSetupTypeChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_user_data(singlesBtn, reinterpret_cast<void*>(static_cast<intptr_t>(GameType::Singles)));
    lv_obj_t* singlesLabel = lv_label_create(singlesBtn);
    lv_label_set_text(singlesLabel, "Singles");

    lv_obj_t* doublesBtn = lv_btn_create(typeRow);
    lv_obj_add_flag(doublesBtn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(doublesBtn, onSetupTypeChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_user_data(doublesBtn, reinterpret_cast<void*>(static_cast<intptr_t>(GameType::Doubles)));
    lv_obj_t* doublesLabel = lv_label_create(doublesBtn);
    lv_label_set_text(doublesLabel, "Doubles");

    nameInputsContainer_ = lv_obj_create(screen);
    lv_obj_set_size(nameInputsContainer_, LV_PCT(90), 140);
    lv_obj_align(nameInputsContainer_, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_flex_flow(nameInputsContainer_, LV_FLEX_FLOW_ROW_WRAP);
    rebuildNameInputs(nameInputsContainer_);

    keyboard_ = lv_keyboard_create(screen);
    lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* startBtn = addButton(screen, "Start Game", onSetupStart);
    lv_obj_align(startBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    lv_obj_t* backBtn = addButton(screen, "Back", onBackToMenu);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
}

void AppController::onSetupTypeChanged(lv_event_t* e) {
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    GameType type = static_cast<GameType>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));

    // Emulate radio-button behaviour: only one of the two type buttons may
    // be checked at a time.
    lv_obj_t* row = lv_obj_get_parent(btn);
    const uint32_t childCount = lv_obj_get_child_cnt(row);
    for (uint32_t i = 0; i < childCount; ++i) {
        lv_obj_t* child = lv_obj_get_child(row, i);
        if (child != btn) {
            lv_obj_clear_state(child, LV_STATE_CHECKED);
        }
    }
    lv_obj_add_state(btn, LV_STATE_CHECKED);

    s_instance->pendingType_ = type;
    s_instance->rebuildNameInputs(s_instance->nameInputsContainer_);
}

void AppController::onSetupStart(lv_event_t*) {
    AppController* self = s_instance;
    const int playersPerTeam = playersPerSide(self->pendingType_);

    std::vector<std::string> team1Names;
    std::vector<std::string> team2Names;
    for (int i = 0; i < playersPerTeam; ++i) {
        const char* text = lv_textarea_get_text(self->nameInputs_[i]);
        char fallback[16];
        std::snprintf(fallback, sizeof(fallback), "Player %d", i + 1);
        team1Names.push_back((text != nullptr && text[0] != '\0') ? text : fallback);
    }
    for (int i = 0; i < playersPerTeam; ++i) {
        const char* text = lv_textarea_get_text(self->nameInputs_[playersPerTeam + i]);
        char fallback[16];
        std::snprintf(fallback, sizeof(fallback), "Player %d", playersPerTeam + i + 1);
        team2Names.push_back((text != nullptr && text[0] != '\0') ? text : fallback);
    }

    self->startNewGame(self->pendingType_, team1Names, team2Names);
}

void AppController::startNewGame(GameType type, const std::vector<std::string>& team1Names,
                                  const std::vector<std::string>& team2Names) {
    currentGame_ = std::unique_ptr<BowlsGame>(new BowlsGame(
        type, team1Names, team2Names, static_cast<uint32_t>(lv_tick_get() / 1000)));
    showScoring();
}

// ---------------------------------------------------------------------------
// Scoring screen
// ---------------------------------------------------------------------------

void AppController::showScoring() {
    lv_obj_t* screen = createScreen();
    addTitle(screen, "Scoring");

    team1ScoreLabel_ = lv_label_create(screen);
    lv_obj_align(team1ScoreLabel_, LV_ALIGN_TOP_LEFT, 20, 40);

    team2ScoreLabel_ = lv_label_create(screen);
    lv_obj_align(team2ScoreLabel_, LV_ALIGN_TOP_RIGHT, -20, 40);

    endCountLabel_ = lv_label_create(screen);
    lv_obj_align(endCountLabel_, LV_ALIGN_TOP_MID, 0, 40);

    const BowlsGame& game = *currentGame_;
    const int maxPerEnd = game.maxPerEnd();

    lv_obj_t* team1Row = lv_obj_create(screen);
    lv_obj_set_size(team1Row, LV_PCT(45), 60);
    lv_obj_align(team1Row, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_flex_flow(team1Row, LV_FLEX_FLOW_ROW_WRAP);
    for (int score = 1; score <= maxPerEnd; ++score) {
        char text[4];
        std::snprintf(text, sizeof(text), "%d", score);
        lv_obj_t* btn = addButton(team1Row, text, onScoreButton);
        // Encode (team=1, score) into the button's user data.
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(static_cast<intptr_t>(100 + score)));
    }

    lv_obj_t* team2Row = lv_obj_create(screen);
    lv_obj_set_size(team2Row, LV_PCT(45), 60);
    lv_obj_align(team2Row, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_flex_flow(team2Row, LV_FLEX_FLOW_ROW_WRAP);
    for (int score = 1; score <= maxPerEnd; ++score) {
        char text[4];
        std::snprintf(text, sizeof(text), "%d", score);
        lv_obj_t* btn = addButton(team2Row, text, onScoreButton);
        // Encode (team=2, score) into the button's user data.
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(static_cast<intptr_t>(200 + score)));
    }

    lv_obj_t* undoBtn = addButton(screen, "Undo End", onUndoEnd);
    lv_obj_align(undoBtn, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    lv_obj_t* endGameBtn = addButton(screen, "End Game", onEndGame);
    lv_obj_align(endGameBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    // Refresh labels with current (zero) scores.
    refreshScoreLabels();
}

void AppController::onScoreButton(lv_event_t* e) {
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const intptr_t encoded = reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn));
    const int team = static_cast<int>(encoded / 100);
    const int score = static_cast<int>(encoded % 100);
    if (team == 1) {
        s_instance->recordEnd(score, 0);
    } else {
        s_instance->recordEnd(0, score);
    }
}

void AppController::recordEnd(int team1Score, int team2Score) {
    currentGame_->recordEnd(team1Score, team2Score);
    refreshScoreLabels();
}

void AppController::refreshScoreLabels() {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Team 1: %d", currentGame_->team1().score);
    lv_label_set_text(team1ScoreLabel_, buf);
    std::snprintf(buf, sizeof(buf), "Team 2: %d", currentGame_->team2().score);
    lv_label_set_text(team2ScoreLabel_, buf);
    std::snprintf(buf, sizeof(buf), "End %d", currentGame_->endCount());
    lv_label_set_text(endCountLabel_, buf);
}

void AppController::onUndoEnd(lv_event_t*) {
    s_instance->currentGame_->undoLastEnd();
    s_instance->refreshScoreLabels();
}

void AppController::onEndGame(lv_event_t*) {
    s_instance->endCurrentGame();
}

void AppController::endCurrentGame() {
    currentGame_->finish(static_cast<uint32_t>(lv_tick_get() / 1000));
    history_.addGame(*currentGame_);
    storage_.save(history_);
    currentGame_.reset();
    showMenu();
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

void AppController::showHistoryList() {
    lv_obj_t* screen = createScreen();
    addTitle(screen, "Old Scores");

    lv_obj_t* list = lv_list_create(screen);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(80));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);

    for (size_t i = 0; i < history_.count(); ++i) {
        const BowlsGame& game = history_.at(i);
        char text[64];
        std::snprintf(text, sizeof(text), "%s vs %s : %d - %d",
                      game.team1().playerNames.empty() ? "" : game.team1().playerNames[0].c_str(),
                      game.team2().playerNames.empty() ? "" : game.team2().playerNames[0].c_str(),
                      game.team1().score, game.team2().score);
        lv_obj_t* btn = lv_list_add_btn(list, nullptr, text);
        lv_obj_add_event_cb(btn, onHistoryItemClicked, LV_EVENT_CLICKED, nullptr);
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }

    lv_obj_t* backBtn = addButton(screen, "Back", onBackToMenu);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppController::onHistoryItemClicked(lv_event_t* e) {
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const size_t index = static_cast<size_t>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));
    s_instance->showHistoryDetail(index);
}

void AppController::showHistoryDetail(size_t index) {
    lv_obj_t* screen = createScreen();
    const BowlsGame& game = history_.at(index);

    char title[32];
    std::snprintf(title, sizeof(title), "%s Game",
                  game.type() == GameType::Doubles ? "Doubles" : "Singles");
    addTitle(screen, title);

    lv_obj_t* summary = lv_label_create(screen);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Team 1: %d\nTeam 2: %d\nEnds played: %d\n%s",
                  game.team1().score, game.team2().score, game.endCount(),
                  game.resultSummary().c_str());
    lv_label_set_text(summary, buf);
    lv_obj_align(summary, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t* backBtn = addButton(screen, "Back", onMenuHistory);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppController::onBackToMenu(lv_event_t*) {
    s_instance->showMenu();
}

}  // namespace bowls
