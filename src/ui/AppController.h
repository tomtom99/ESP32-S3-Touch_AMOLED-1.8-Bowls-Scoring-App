#pragma once

#include <lvgl.h>

#include <cstdint>
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

    // Registers a callback used to change the physical display brightness.
    // Must be called before begin() to have the saved brightness applied
    // on startup.
    void setBrightnessSetter(void (*setter)(uint8_t));

    // Registers a callback used to read the current battery percentage
    // (0-100), or a negative value if unavailable. Drives a small label
    // shown in the top-right corner of every screen.
    void setBatteryPercentGetter(int (*getter)());

    void setScoreAnnouncer(void (*announcer)(int homeScore, int awayScore, bool deadEnd));
    void setAudioVolumeSetter(void (*setter)(uint8_t volumePercent));

    // Loads any saved history from storage and shows the main menu.
    void begin();

    // Dims the display to off without touching the saved brightness setting
    // (used when the PWR button puts the screen to sleep).
    void enterDisplaySleep();
    // Restores the display to its saved brightness (used when the PWR
    // button wakes the screen back up).
    void exitDisplaySleep();
    bool isDisplaySleeping() const { return displaySleeping_; }

private:
    // --- Screen builders -------------------------------------------------
    void showMenu();
    void showNewGameSetup();
    void showWinningScoreSetup();
    void showHandicapSetup();
    void showScoring();
    void showHistoryList();
    void showHistoryDetail(size_t index);
    void showSettings();

    // --- Actions -----------------------------------------------------------
    void startNewGame();
    void recordEnd(int team1Score, int team2Score);
    void recordDeadEnd();
    void endCurrentGame();
    void refreshEndsTable();
    void updateSliderLabel(int32_t value);
    void activateEndGameSlider();
    void applyBrightness();
    void pushBrightnessToHardware();
    void applyAudioVolume();
    void resetUserData();

    // Battery indicator (top-right, drawn on the LVGL top layer so it
    // survives screen switches).
    void createBatteryLabel();
    void updateBatteryLabel();
    static void onBatteryTimer(lv_timer_t* timer);

    // Long-press context menu for editing/deleting a single end.
    void showEndMenu(int endIndex);
    void closeEndMenu();

    // Confirmation overlay shown before wiping all saved data.
    void showResetConfirm();
    void closeResetConfirm();

    // Fills a read-only or live ends table (Scr/Tot columns) for a game.
    static void populateEndsTable(lv_obj_t* table, const BowlsGame& game);

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
    static void onMenuSettings(lv_event_t* e);
    static void onSettingsBrightnessChanged(lv_event_t* e);
    static void onSettingsAudioVolumeChanged(lv_event_t* e);
    static void onSettingsResetRequested(lv_event_t* e);
    static void onResetConfirmYes(lv_event_t* e);
    static void onResetConfirmCancel(lv_event_t* e);

    static AppController* s_instance;

    GameStorage& storage_;
    GameHistory history_;
    std::unique_ptr<BowlsGame> currentGame_;
    GameType pendingType_ = GameType::Singles;
    int pendingWinningScore_ = 21;
    int pendingHomeHandicap_ = 0;
    int pendingAwayHandicap_ = 0;
    bool hasInProgressGame_ = false;
    bool awaitingEndConfirmation_ = false;

    lv_obj_t* setupSinglesButton_ = nullptr;
    lv_obj_t* setupDoublesButton_ = nullptr;
    lv_obj_t* setupWinningScoreSlider_ = nullptr;
    lv_obj_t* setupHomeHandicapSlider_ = nullptr;
    lv_obj_t* setupAwayHandicapSlider_ = nullptr;
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
    lv_obj_t* recordSliderHint_ = nullptr;
    bool recordSliderHintAnimationStarted_ = false;
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

    // Settings screen.
    lv_obj_t* settingsBrightnessSlider_ = nullptr;
    lv_obj_t* settingsBrightnessLabel_ = nullptr;
    lv_obj_t* settingsAudioVolumeSlider_ = nullptr;
    lv_obj_t* settingsAudioVolumeLabel_ = nullptr;
    int brightnessPercent_ = 80;
    int audioVolumePercent_ = 35;
    void (*brightnessSetter_)(uint8_t) = nullptr;
    void (*audioVolumeSetter_)(uint8_t volumePercent) = nullptr;
    bool displaySleeping_ = false;

    // Battery percentage indicator.
    lv_obj_t* batteryLabel_ = nullptr;
    int (*batteryPercentGetter_)() = nullptr;

    void (*scoreAnnouncer_)(int homeScore, int awayScore, bool deadEnd) = nullptr;

    // "Are you sure?" overlay shown before resetting all saved data.
    lv_obj_t* resetConfirmOverlay_ = nullptr;
};

}  // namespace bowls
