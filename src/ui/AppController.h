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
    void showWinningScoreSetup();
    void showHandicapSetup();
    void showScoring();
    void showHistoryList();
    void showHistoryDetail(size_t index);

    // --- Actions -----------------------------------------------------------
    void startNewGame();
    void recordEnd(int team1Score, int team2Score);
    void recordDeadEnd();
    void endCurrentGame();
    void refreshEndsTable();
    void updateSliderLabel(int32_t value);
    void activateEndGameSlider();

    // Long-press context menu for editing/deleting a single end.
    void showEndMenu(int endIndex);
    void closeEndMenu();

    // Renders the Scr/Tot cells with the largest available font.
    static void onEndsTableDrawPart(lv_event_t* e);

    // --- Helpers -------------------------------------------------------
    static lv_obj_t* createScreen();
    static lv_obj_t* addTitle(lv_obj_t* parent, const char* text);
    static lv_obj_t* addButton(lv_obj_t* parent, const char* text,
                                lv_event_cb_t callback);

    // --- Event trampolines (LVGL callbacks are plain C function pointers).
    // The app only ever has a single AppController instance (s_instance),
    // so callbacks dispatch to it directly instead of threading `this`
    // through every LVGL user-data slot.
    static void onMenuNewGame(lv_event_t* e);
    static void onMenuContinue(lv_event_t* e);
    static void onMenuHistory(lv_event_t* e);
    static void onSetupTypeChanged(lv_event_t* e);
    static void onSetupWinningScoreChanged(lv_event_t* e);
    static void onSetupHandicapChanged(lv_event_t* e);
    static void onSetupNext(lv_event_t* e);
    static void onSliderChanged(lv_event_t* e);
    static void onRecordEnd(lv_event_t* e);
    static void onRecordSliderPressed(lv_event_t* e);
    static void onRecordSliderReleased(lv_event_t* e);
    static void onEndGameSliderPressed(lv_event_t* e);
    static void onEndGameSliderReleased(lv_event_t* e);
    static void onEndsTableLongPressed(lv_event_t* e);
    static void onEndMenuEdit(lv_event_t* e);
    static void onEndMenuDelete(lv_event_t* e);
    static void onEndMenuCancel(lv_event_t* e);
    static void onBackToMenu(lv_event_t* e);
    static void onHistoryItemClicked(lv_event_t* e);

    static AppController* s_instance;

    GameStorage& storage_;
    GameHistory history_;
    std::unique_ptr<BowlsGame> currentGame_;
    GameType pendingType_ = GameType::Singles;
    int pendingWinningScore_ = 11;
    int pendingHomeHandicap_ = 0;
    int pendingAwayHandicap_ = 0;
    bool hasInProgressGame_ = false;
    bool awaitingEndConfirmation_ = false;

    lv_obj_t* setupTypeSlider_ = nullptr;
    lv_obj_t* setupWinningScoreSlider_ = nullptr;
    lv_obj_t* setupHomeHandicapSlider_ = nullptr;
    lv_obj_t* setupAwayHandicapSlider_ = nullptr;
    lv_obj_t* setupTypeLabel_ = nullptr;
    lv_obj_t* setupWinningScoreLabel_ = nullptr;
    lv_obj_t* setupHomeHandicapLabel_ = nullptr;
    lv_obj_t* setupAwayHandicapLabel_ = nullptr;

    // Ends table + input slider on the scoring screen.
    lv_obj_t* endsTableContainer_ = nullptr;
    lv_obj_t* endsTable_ = nullptr;
    lv_obj_t* slider_ = nullptr;
    lv_obj_t* sliderLabel_ = nullptr;

    // "Slide to record" confirmation control at the bottom of the screen.
    lv_obj_t* recordSlider_ = nullptr;
    lv_obj_t* recordSliderLabel_ = nullptr;
    int32_t recordSliderPressX_ = 0;

    // "Swipe to end game" control across the top of the scoring screen.
    lv_obj_t* endGameSlider_ = nullptr;
    lv_obj_t* endGameSliderLabel_ = nullptr;
    int32_t endGameSliderPressX_ = 0;

    // Set to a non-negative end index while the record slider is being used
    // to edit an existing end rather than record a new one.
    int editingEndIndex_ = -1;

    // Long-press context menu overlay for a specific end (edit/delete).
    lv_obj_t* endMenuOverlay_ = nullptr;
    int endMenuIndex_ = -1;
};

}  // namespace bowls
