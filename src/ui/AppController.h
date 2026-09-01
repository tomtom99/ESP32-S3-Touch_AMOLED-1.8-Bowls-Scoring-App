#pragma once

#include <lvgl.h>

#include <memory>

#include "core/BowlsGame.h"
#include "core/GameHistory.h"
#include "core/GameStorage.h"

namespace bowls {

// Owns the app state (game history + in-progress game) and drives all of
// the LVGL screens: main menu, new game setup, live scoring and history
// browsing. Screen building is split into private helpers for readability.
class AppController {
public:
    explicit AppController(GameStorage& storage);

    // Loads any saved history from storage and shows the main menu.
    void begin();

private:
    // --- Screen builders -------------------------------------------------
    void showMenu();
    void showNewGameSetup();
    void showScoring();
    void showHistoryList();
    void showHistoryDetail(size_t index);

    // --- Actions -----------------------------------------------------------
    void startNewGame(GameType type, const std::vector<std::string>& team1Names,
                       const std::vector<std::string>& team2Names);
    void recordEnd(int team1Score, int team2Score);
    void endCurrentGame();
    void refreshScoreLabels();

    // --- Helpers -------------------------------------------------------
    static lv_obj_t* createScreen();
    static lv_obj_t* addTitle(lv_obj_t* parent, const char* text);
    static lv_obj_t* addButton(lv_obj_t* parent, const char* text,
                                lv_event_cb_t callback);
    void rebuildNameInputs(lv_obj_t* container);

    // --- Event trampolines (LVGL callbacks are plain C function pointers).
    // The app only ever has a single AppController instance (s_instance),
    // so callbacks dispatch to it directly instead of threading `this`
    // through every LVGL user-data slot.
    static void onMenuNewGame(lv_event_t* e);
    static void onMenuHistory(lv_event_t* e);
    static void onSetupTypeChanged(lv_event_t* e);
    static void onSetupStart(lv_event_t* e);
    static void onScoreButton(lv_event_t* e);
    static void onUndoEnd(lv_event_t* e);
    static void onEndGame(lv_event_t* e);
    static void onBackToMenu(lv_event_t* e);
    static void onHistoryItemClicked(lv_event_t* e);
    static void onTextAreaFocused(lv_event_t* e);

    static AppController* s_instance;

    GameStorage& storage_;
    GameHistory history_;
    std::unique_ptr<BowlsGame> currentGame_;
    GameType pendingType_ = GameType::Singles;

    // Text areas for entering player names on the setup screen.
    lv_obj_t* nameInputs_[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nameInputsContainer_ = nullptr;
    lv_obj_t* keyboard_ = nullptr;

    // Labels on the scoring screen that need to be refreshed after
    // recording/undoing an end.
    lv_obj_t* team1ScoreLabel_ = nullptr;
    lv_obj_t* team2ScoreLabel_ = nullptr;
    lv_obj_t* endCountLabel_ = nullptr;
};

}  // namespace bowls
