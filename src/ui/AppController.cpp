#include "ui/AppController.h"

#include <cstdio>

namespace bowls {

namespace {

constexpr lv_coord_t kButtonWidth = 170;
constexpr lv_coord_t kButtonHeight = 64;
constexpr lv_coord_t kMenuButtonWidth = 330;
constexpr lv_coord_t kSetupButtonWidth = 172;
constexpr lv_coord_t kSetupButtonHeight = 92;
constexpr lv_coord_t kStartButtonWidth = 210;
constexpr lv_coord_t kStartButtonHeight = 86;

// "Slide to record"/"Swipe to end game" controls only count as a deliberate
// swipe (rather than a stray tap landing far enough along the track to look
// like a completed drag) once the touch point has moved at least this many
// pixels horizontally between press and release.
constexpr int32_t kMinSwipeDistance = 60;

// "Slide to record" control: dragging the knob past this fraction of the
// track confirms the action; releasing early snaps the knob back to zero.
constexpr int32_t kRecordSliderMax = 100;
constexpr int32_t kRecordThreshold = 85;

// Brightness is stored/adjusted as a percentage; never let it drop so low
// the screen becomes unreadable/unusable.
constexpr int kMinBrightnessPercent = 10;
constexpr int kDefaultBrightnessPercent = 80;
constexpr int kDefaultAudioVolumePercent = 35;

// How often the battery percentage label is refreshed.
constexpr uint32_t kBatteryPollIntervalMs = 30000;

constexpr uint16_t kRecordHintDurationMs = 1200;

void animateRecordSliderHint(void* hint, int32_t progress) {
    lv_obj_t* hintLabel = static_cast<lv_obj_t*>(hint);
    lv_obj_t* slider = lv_obj_get_parent(hintLabel);
    const lv_coord_t travel = lv_obj_get_width(slider) - lv_obj_get_width(hintLabel) - 32;
    const lv_coord_t offset = 16 + (travel * progress) / 100;
    lv_obj_align(hintLabel, LV_ALIGN_LEFT_MID, offset, 0);
}

void styleScreen(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(obj, 8, 0);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_text_color(obj, lv_color_black(), 0);
}

void stylePanel(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(obj, 8, 0);
    lv_obj_set_style_pad_row(obj, 8, 0);
    lv_obj_set_style_pad_column(obj, 8, 0);
    lv_obj_set_style_bg_color(obj, lv_palette_lighten(LV_PALETTE_GREY, 4), 0);
    lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_color(obj, lv_color_black(), 0);
}

void styleButton(lv_obj_t* btn, lv_coord_t width = kButtonWidth, lv_coord_t height = kButtonHeight) {
    lv_obj_set_size(btn, width, height);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_pad_all(btn, 8, 0);
    lv_obj_set_style_text_font(btn, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(btn, lv_color_black(), 0);
    lv_obj_set_style_bg_color(btn, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_GREY, 1), LV_STATE_CHECKED);
    lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
}

void styleForwardButton(lv_obj_t* btn) {
    lv_obj_set_style_bg_color(btn, lv_palette_lighten(LV_PALETTE_GREEN, 3), 0);
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_GREEN, 2), 0);
}

void styleBodyLabel(lv_obj_t* label) {
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
}

// Fills buf with the display text for a given slider position, where
// `center` is the slider value that represents a dead end (i.e. the value
// halfway along the track, equal to the game's maxPerEnd()).
void sliderValueText(char* buf, size_t bufSize, int32_t value, int32_t center) {
    const int32_t diff = value - center;
    if (diff == 0) {
        std::snprintf(buf, bufSize, "DEAD END");
    } else {
        const int32_t homeScore = diff > 0 ? diff : 0;
        const int32_t awayScore = diff < 0 ? -diff : 0;
        std::snprintf(buf, bufSize, "HOME %ld - %ld AWAY", static_cast<long>(homeScore),
                      static_cast<long>(awayScore));
    }
}

}  // namespace

AppController* AppController::s_instance = nullptr;

AppController::AppController(GameStorage& storage) : storage_(storage) {
    s_instance = this;
}

void AppController::setBrightnessSetter(void (*setter)(uint8_t)) {
    brightnessSetter_ = setter;
}

void AppController::setBatteryPercentGetter(int (*getter)()) {
    batteryPercentGetter_ = getter;
}

void AppController::setScoreAnnouncer(void (*announcer)(int homeScore, int awayScore, bool deadEnd)) {
    scoreAnnouncer_ = announcer;
}

void AppController::setAudioVolumeSetter(void (*setter)(uint8_t volumePercent)) {
    audioVolumeSetter_ = setter;
}

void AppController::begin() {
    storage_.load(history_);
    BowlsGame savedGame;
    if (storage_.loadInProgress(savedGame) && !savedGame.isFinished()) {
        currentGame_ = std::unique_ptr<BowlsGame>(new BowlsGame(std::move(savedGame)));
        hasInProgressGame_ = true;
    }

    uint8_t savedBrightness = 0;
    if (storage_.loadBrightness(savedBrightness)) {
        brightnessPercent_ = (static_cast<int>(savedBrightness) * 100) / 255;
        if (brightnessPercent_ < kMinBrightnessPercent) brightnessPercent_ = kMinBrightnessPercent;
    }
    if (brightnessSetter_ != nullptr) {
        brightnessSetter_(static_cast<uint8_t>((brightnessPercent_ * 255) / 100));
    }

    uint8_t savedAudioVolume = 0;
    if (storage_.loadAudioVolume(savedAudioVolume)) {
        audioVolumePercent_ = savedAudioVolume;
    }
    if (audioVolumeSetter_ != nullptr) {
        audioVolumeSetter_(static_cast<uint8_t>(audioVolumePercent_));
    }

    createBatteryLabel();

    showMenu();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

lv_obj_t* AppController::createScreen() {
    lv_obj_t* screen = lv_obj_create(nullptr);
    styleScreen(screen);
    lv_scr_load(screen);
    return screen;
}

// Battery label lives on the LVGL top layer (not a screen), so it stays
// visible across every lv_scr_load() call without needing to be re-added.
void AppController::createBatteryLabel() {
    lv_obj_t* layer = lv_layer_top();
    batteryLabel_ = lv_label_create(layer);
    lv_obj_set_style_text_font(batteryLabel_, &lv_font_montserrat_20, 0);
    lv_obj_align(batteryLabel_, LV_ALIGN_TOP_RIGHT, -36, 6);
    lv_label_set_text(batteryLabel_, "");

    updateBatteryLabel();
    lv_timer_create(onBatteryTimer, kBatteryPollIntervalMs, nullptr);
}

void AppController::updateBatteryLabel() {
    if (batteryLabel_ == nullptr) return;
    int percent = batteryPercentGetter_ != nullptr ? batteryPercentGetter_() : -1;
    if (percent < 0 || percent > 100) {
        lv_label_set_text(batteryLabel_, "");
        return;
    }

    const char* icon = LV_SYMBOL_BATTERY_EMPTY;
    if (percent >= 80) {
        icon = LV_SYMBOL_BATTERY_FULL;
    } else if (percent >= 55) {
        icon = LV_SYMBOL_BATTERY_3;
    } else if (percent >= 30) {
        icon = LV_SYMBOL_BATTERY_2;
    } else if (percent >= 15) {
        icon = LV_SYMBOL_BATTERY_1;
    }

    // Red/amber/green traffic-light coloring so charge state is visible at a glance.
    lv_color_t color = percent >= 40 ? lv_palette_main(LV_PALETTE_GREEN)
                        : percent >= 15 ? lv_palette_main(LV_PALETTE_ORANGE)
                                        : lv_palette_main(LV_PALETTE_RED);
    lv_obj_set_style_text_color(batteryLabel_, color, 0);
    lv_label_set_text(batteryLabel_, icon);
    lv_obj_align(batteryLabel_, LV_ALIGN_TOP_RIGHT, -36, 6);
}

void AppController::onBatteryTimer(lv_timer_t* timer) {
    (void)timer;
    if (s_instance != nullptr) {
        s_instance->updateBatteryLabel();
    }
}

lv_obj_t* AppController::addTitle(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    return label;
}

lv_obj_t* AppController::addButton(lv_obj_t* parent, const char* text, lv_event_cb_t callback) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_add_event_cb(btn, callback, LV_EVENT_PRESSED, nullptr);
    styleButton(btn);
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
    lv_obj_t* title = addTitle(screen, "Crown Green Bowls");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);

    lv_obj_t* newGameBtn = addButton(screen, "New Game", onMenuNewGame);
    styleButton(newGameBtn, kMenuButtonWidth, 70);
    styleForwardButton(newGameBtn);
    lv_obj_set_style_text_font(newGameBtn, &lv_font_montserrat_28, 0);
    lv_obj_align(newGameBtn, LV_ALIGN_CENTER, 0, hasInProgressGame_ ? -74 : -44);

    if (hasInProgressGame_) {
        lv_obj_t* continueBtn = addButton(screen, "Continue Game", onMenuContinue);
        styleButton(continueBtn, kMenuButtonWidth, 70);
        lv_obj_set_style_bg_color(continueBtn, lv_palette_lighten(LV_PALETTE_ORANGE, 3), 0);
        lv_obj_set_style_bg_color(continueBtn, lv_palette_main(LV_PALETTE_ORANGE), LV_STATE_PRESSED);
        lv_obj_set_style_text_font(continueBtn, &lv_font_montserrat_28, 0);
        lv_obj_align(continueBtn, LV_ALIGN_CENTER, 0, 4);
    }

    lv_obj_t* historyBtn = addButton(screen, "View Old Scores", onMenuHistory);
    styleButton(historyBtn, kMenuButtonWidth, 70);
    lv_obj_set_style_bg_color(historyBtn, lv_palette_lighten(LV_PALETTE_YELLOW, 3), 0);
    lv_obj_set_style_bg_color(historyBtn, lv_palette_main(LV_PALETTE_YELLOW), LV_STATE_PRESSED);
    lv_obj_set_style_text_font(historyBtn, &lv_font_montserrat_28, 0);
    lv_obj_align(historyBtn, LV_ALIGN_CENTER, 0, hasInProgressGame_ ? 82 : 44);

    lv_obj_t* settingsBtn = addButton(screen, "Settings", onMenuSettings);
    styleButton(settingsBtn, kMenuButtonWidth, 70);
    lv_obj_set_style_text_font(settingsBtn, &lv_font_montserrat_28, 0);
    lv_obj_align(settingsBtn, LV_ALIGN_CENTER, 0, hasInProgressGame_ ? 160 : 122);
}

void AppController::onMenuNewGame(lv_event_t*) {
    s_instance->showNewGameSetup();
}

void AppController::onMenuContinue(lv_event_t*) {
    s_instance->showScoring();
}

void AppController::onMenuHistory(lv_event_t*) {
    s_instance->showHistoryList();
}

// ---------------------------------------------------------------------------
// New game setup
// ---------------------------------------------------------------------------

void AppController::showNewGameSetup() {
    pendingType_ = GameType::Singles;
    setupSinglesButton_ = nullptr;
    setupDoublesButton_ = nullptr;
    setupWinningScoreSlider_ = nullptr;
    setupHomeHandicapSlider_ = nullptr;
    setupAwayHandicapSlider_ = nullptr;
    lv_obj_t* screen = createScreen();
    addTitle(screen, "Game Type");

    lv_obj_t* subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Choose players per team");
    styleBodyLabel(subtitle);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 46);

    setupSinglesButton_ = addButton(screen, "Singles", onSetupTypeChanged);
    styleButton(setupSinglesButton_, 260, 64);
    lv_obj_add_state(setupSinglesButton_, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(setupSinglesButton_, lv_palette_lighten(LV_PALETTE_GREEN, 3), 0);
    lv_obj_set_style_bg_color(setupSinglesButton_, lv_palette_main(LV_PALETTE_GREEN),
                              LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(setupSinglesButton_, lv_palette_darken(LV_PALETTE_GREEN, 1),
                              LV_STATE_PRESSED);
    lv_obj_align(setupSinglesButton_, LV_ALIGN_TOP_MID, 0, 88);

    setupDoublesButton_ = addButton(screen, "Doubles", onSetupTypeChanged);
    styleButton(setupDoublesButton_, 260, 64);
    lv_obj_set_style_bg_color(setupDoublesButton_, lv_palette_lighten(LV_PALETTE_BLUE, 3), 0);
    lv_obj_set_style_bg_color(setupDoublesButton_, lv_palette_main(LV_PALETTE_BLUE),
                              LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(setupDoublesButton_, lv_palette_darken(LV_PALETTE_BLUE, 1),
                              LV_STATE_PRESSED);
    lv_obj_align(setupDoublesButton_, LV_ALIGN_TOP_MID, 0, 164);

    lv_obj_t* nextBtn = addButton(screen, "Next", onSetupNext);
    styleButton(nextBtn, kStartButtonWidth, kButtonHeight);
    styleForwardButton(nextBtn);
    lv_obj_align(nextBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    lv_obj_t* backBtn = addButton(screen, "Back", onBackToMenu);
    styleButton(backBtn, 150, kButtonHeight);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
}

void AppController::onSetupTypeChanged(lv_event_t* e) {
    lv_obj_t* selectedButton = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const bool doublesSelected = selectedButton == s_instance->setupDoublesButton_;
    s_instance->pendingType_ = doublesSelected ? GameType::Doubles : GameType::Singles;
    lv_obj_add_state(selectedButton, LV_STATE_CHECKED);
    lv_obj_clear_state(doublesSelected ? s_instance->setupSinglesButton_
                                       : s_instance->setupDoublesButton_,
                      LV_STATE_CHECKED);
}

void AppController::showWinningScoreSetup() {
    setupSinglesButton_ = nullptr;
    setupDoublesButton_ = nullptr;
    lv_obj_t* screen = createScreen();
    addTitle(screen, "Winning Score");

    lv_obj_t* subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "First team to");
    styleBodyLabel(subtitle);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 46);

    setupWinningScoreSlider_ = lv_slider_create(screen);
    lv_slider_set_range(setupWinningScoreSlider_, 0, 2);
    lv_slider_set_value(setupWinningScoreSlider_, 2, LV_ANIM_OFF);
    lv_obj_set_size(setupWinningScoreSlider_, LV_PCT(88), 54);
    lv_obj_align(setupWinningScoreSlider_, LV_ALIGN_CENTER, 0, -26);
    lv_obj_add_event_cb(setupWinningScoreSlider_, onSetupWinningScoreChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    setupWinningScoreLabel_ = lv_label_create(screen);
    lv_label_set_text(setupWinningScoreLabel_, "21 points");
    lv_obj_set_style_text_font(setupWinningScoreLabel_, &lv_font_montserrat_32, 0);
    lv_obj_align(setupWinningScoreLabel_, LV_ALIGN_CENTER, 0, 38);

    lv_obj_t* nextBtn = addButton(screen, "Next", onSetupNext);
    styleButton(nextBtn, kStartButtonWidth, kButtonHeight);
    styleForwardButton(nextBtn);
    lv_obj_align(nextBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_t* backBtn = addButton(screen, "Back", onBackToMenu);
    styleButton(backBtn, 150, kButtonHeight);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
}

void AppController::onSetupWinningScoreChanged(lv_event_t* e) {
    static constexpr int kScores[] = {11, 15, 21};
    const int32_t value = lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(e)));
    s_instance->pendingWinningScore_ = kScores[value];
    lv_label_set_text_fmt(s_instance->setupWinningScoreLabel_, "%d points", kScores[value]);
}

void AppController::showHandicapSetup() {
    setupWinningScoreSlider_ = nullptr;
    lv_obj_t* screen = createScreen();
    addTitle(screen, "Handicaps");

    setupHomeHandicapLabel_ = lv_label_create(screen);
    lv_label_set_text(setupHomeHandicapLabel_, "Home: 0");
    styleBodyLabel(setupHomeHandicapLabel_);
    lv_obj_align(setupHomeHandicapLabel_, LV_ALIGN_TOP_LEFT, 16, 58);
    setupHomeHandicapSlider_ = lv_slider_create(screen);
    lv_slider_set_range(setupHomeHandicapSlider_, 0, 7);
    lv_obj_set_size(setupHomeHandicapSlider_, LV_PCT(88), 36);
    lv_obj_align(setupHomeHandicapSlider_, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_add_event_cb(setupHomeHandicapSlider_, onSetupHandicapChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    setupAwayHandicapLabel_ = lv_label_create(screen);
    lv_label_set_text(setupAwayHandicapLabel_, "Away: 0");
    styleBodyLabel(setupAwayHandicapLabel_);
    lv_obj_align(setupAwayHandicapLabel_, LV_ALIGN_TOP_LEFT, 16, 152);
    setupAwayHandicapSlider_ = lv_slider_create(screen);
    lv_slider_set_range(setupAwayHandicapSlider_, 0, 7);
    lv_obj_set_size(setupAwayHandicapSlider_, LV_PCT(88), 36);
    lv_obj_align(setupAwayHandicapSlider_, LV_ALIGN_TOP_MID, 0, 184);
    lv_obj_add_event_cb(setupAwayHandicapSlider_, onSetupHandicapChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* startBtn = addButton(screen, "Start Game", onSetupNext);
    styleButton(startBtn, kStartButtonWidth, kButtonHeight);
    styleForwardButton(startBtn);
    lv_obj_align(startBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_t* backBtn = addButton(screen, "Back", onBackToMenu);
    styleButton(backBtn, 150, kButtonHeight);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
}

void AppController::onSetupHandicapChanged(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const int value = lv_slider_get_value(slider);
    if (slider == s_instance->setupHomeHandicapSlider_) {
        s_instance->pendingHomeHandicap_ = value;
        lv_label_set_text_fmt(s_instance->setupHomeHandicapLabel_, "Home: %d", value);
    } else {
        s_instance->pendingAwayHandicap_ = value;
        lv_label_set_text_fmt(s_instance->setupAwayHandicapLabel_, "Away: %d", value);
    }
}

void AppController::onSetupNext(lv_event_t*) {
    if (s_instance->setupSinglesButton_ != nullptr) {
        s_instance->showWinningScoreSetup();
    } else if (s_instance->setupWinningScoreSlider_ != nullptr) {
        s_instance->showHandicapSetup();
    } else {
        s_instance->startNewGame();
    }
}

void AppController::startNewGame() {
    currentGame_ = std::unique_ptr<BowlsGame>(new BowlsGame(
        pendingType_, {"Home"}, {"Away"}, static_cast<uint32_t>(lv_tick_get() / 1000),
        pendingWinningScore_, pendingHomeHandicap_, pendingAwayHandicap_));
    hasInProgressGame_ = true;
    storage_.saveInProgress(*currentGame_);
    showScoring();
}

// ---------------------------------------------------------------------------
// Scoring screen
// ---------------------------------------------------------------------------

void AppController::showScoring() {
    lv_obj_t* screen = createScreen();
    editingEndIndex_ = -1;
    awaitingEndConfirmation_ = currentGame_->hasReachedWinningScore();
    recordSliderHint_ = nullptr;
    recordSliderHintAnimationStarted_ = false;
    const int32_t maxPerEnd = currentGame_->maxPerEnd();

    // Scrollable table of every end played so far, plus running totals.
    // Columns: Home score | Home total | End | Away score | Away total.
    // Long-pressing a row opens a menu to edit or delete that end.
    endsTableContainer_ = lv_obj_create(screen);
    lv_obj_set_style_pad_all(endsTableContainer_, 0, 0);
    // Bottom padding gives scroll-to-bottom room so the last row clears fully.
    lv_obj_set_style_pad_bottom(endsTableContainer_, 8, 0);
    lv_obj_set_scroll_dir(endsTableContainer_, LV_DIR_VER);
    lv_obj_set_size(endsTableContainer_, LV_PCT(100), 250);
    lv_obj_align(endsTableContainer_, LV_ALIGN_TOP_MID, 0, 80);

    endsTable_ = lv_table_create(endsTableContainer_);
    lv_obj_set_style_text_font(endsTable_, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_table_set_col_cnt(endsTable_, 5);
    lv_table_set_col_width(endsTable_, 0, 70);
    lv_table_set_col_width(endsTable_, 1, 86);
    lv_table_set_col_width(endsTable_, 2, 28);
    lv_table_set_col_width(endsTable_, 3, 70);
    lv_table_set_col_width(endsTable_, 4, 86);
    lv_obj_add_event_cb(endsTable_, onEndsTableDrawPart, LV_EVENT_DRAW_PART_BEGIN, nullptr);
    lv_obj_add_event_cb(endsTable_, onEndsTableLongPressed, LV_EVENT_LONG_PRESSED, nullptr);

    // Big, ever-visible slider for choosing the outcome of the current end.
    // Range is 0..2*maxPerEnd so doubles games can score up to 4 up/4 down.
    slider_ = lv_slider_create(screen);
    lv_slider_set_range(slider_, 0, maxPerEnd * 2);
    lv_slider_set_value(slider_, maxPerEnd, LV_ANIM_OFF);
    lv_obj_set_size(slider_, LV_PCT(92), 40);
    lv_obj_align(slider_, LV_ALIGN_TOP_MID, 0, 348);

    sliderLabel_ = lv_label_create(screen);
    lv_obj_set_style_text_font(sliderLabel_, &lv_font_montserrat_32, 0);
    lv_obj_align(sliderLabel_, LV_ALIGN_TOP_MID, 0, 400);
    lv_obj_add_event_cb(slider_, onSliderChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    recordSlider_ = lv_slider_create(screen);
    lv_slider_set_range(recordSlider_, 0, kRecordSliderMax);
    lv_slider_set_value(recordSlider_, 0, LV_ANIM_OFF);
    lv_obj_set_size(recordSlider_, LV_PCT(92), 64);
    lv_obj_align(recordSlider_, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_radius(recordSlider_, 32, 0);
    lv_obj_set_style_bg_color(recordSlider_, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(recordSlider_, lv_palette_darken(LV_PALETTE_GREY, 1), LV_PART_INDICATOR);
    lv_obj_set_style_radius(recordSlider_, 32, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(recordSlider_, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_radius(recordSlider_, 28, LV_PART_KNOB);
    lv_obj_set_style_pad_all(recordSlider_, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(recordSlider_, onRecordSliderPressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(recordSlider_, onRecordSliderReleased, LV_EVENT_RELEASED, nullptr);

    recordSliderLabel_ = lv_label_create(recordSlider_);
    lv_label_set_text(recordSliderLabel_, "RECORD END");
    lv_obj_set_style_text_font(recordSliderLabel_, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(recordSliderLabel_, lv_color_black(), 0);
    lv_obj_clear_flag(recordSliderLabel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(recordSliderLabel_);

    recordSliderHint_ = lv_label_create(recordSlider_);
    lv_label_set_text(recordSliderHint_, ">>");
    lv_obj_set_style_text_font(recordSliderHint_, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(recordSliderHint_, lv_color_black(), 0);
    lv_obj_clear_flag(recordSliderHint_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(recordSliderHint_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(recordSliderHint_, LV_ALIGN_LEFT_MID, 16, 0);

    refreshEndsTable();
    if (awaitingEndConfirmation_) {
        activateEndGameSlider();
    } else {
        updateSliderLabel(maxPerEnd);
    }
}

void AppController::updateSliderLabel(int32_t value) {
    if (awaitingEndConfirmation_) return;
    const int32_t center = currentGame_->maxPerEnd();
    char buf[16];
    sliderValueText(buf, sizeof(buf), center * 2 - value, center);
    lv_label_set_text(sliderLabel_, buf);
    const lv_color_t color = value > center ? lv_palette_main(LV_PALETTE_RED)
                                             : value < center ? lv_palette_main(LV_PALETTE_GREEN)
                                                              : lv_palette_main(LV_PALETTE_GREY);
    // Color both the indicator and the track so the whole bar reflects the
    // outcome (e.g. fully grey on a dead end, fully green on a home win)
    // rather than only the portion between the knob and one end.
    lv_obj_set_style_bg_color(slider_, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_, color, LV_PART_MAIN);
    lv_obj_set_style_text_color(sliderLabel_, color, 0);
}

void AppController::onSliderChanged(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const int32_t value = lv_slider_get_value(slider);
    s_instance->updateSliderLabel(value);

    if (!s_instance->recordSliderHintAnimationStarted_ &&
        s_instance->recordSliderHint_ != nullptr && value != s_instance->currentGame_->maxPerEnd()) {
        s_instance->recordSliderHintAnimationStarted_ = true;
        lv_obj_clear_flag(s_instance->recordSliderHint_, LV_OBJ_FLAG_HIDDEN);
        lv_anim_t recordHintAnimation;
        lv_anim_init(&recordHintAnimation);
        lv_anim_set_var(&recordHintAnimation, s_instance->recordSliderHint_);
        lv_anim_set_values(&recordHintAnimation, 0, 100);
        lv_anim_set_time(&recordHintAnimation, kRecordHintDurationMs);
        lv_anim_set_repeat_count(&recordHintAnimation, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&recordHintAnimation, animateRecordSliderHint);
        lv_anim_start(&recordHintAnimation);
    }
}

void AppController::onRecordEnd(lv_event_t*) {
    lv_anim_del(s_instance->recordSliderHint_, animateRecordSliderHint);
    s_instance->recordSliderHintAnimationStarted_ = false;
    lv_obj_add_flag(s_instance->recordSliderHint_, LV_OBJ_FLAG_HIDDEN);

    const int32_t maxPerEnd = s_instance->currentGame_->maxPerEnd();
    const int32_t value = lv_slider_get_value(s_instance->slider_);
    const int32_t diff = maxPerEnd - value;
    const int team1Score = diff > 0 ? static_cast<int>(diff) : 0;
    const int team2Score = diff < 0 ? static_cast<int>(-diff) : 0;

    if (s_instance->editingEndIndex_ >= 0) {
        s_instance->currentGame_->editEnd(static_cast<size_t>(s_instance->editingEndIndex_),
                                          team1Score, team2Score);
        s_instance->editingEndIndex_ = -1;
        lv_label_set_text(s_instance->recordSliderLabel_, "RECORD END");
        s_instance->refreshEndsTable();
        s_instance->storage_.saveInProgress(*s_instance->currentGame_);
        if (s_instance->currentGame_->hasReachedWinningScore()) {
            s_instance->activateEndGameSlider();
        }
    } else if (diff == 0) {
        s_instance->recordDeadEnd();
    } else {
        s_instance->recordEnd(team1Score, team2Score);
    }

    lv_slider_set_value(s_instance->slider_, maxPerEnd, LV_ANIM_OFF);
    s_instance->updateSliderLabel(maxPerEnd);
}

void AppController::onRecordSliderPressed(lv_event_t*) {
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    s_instance->recordSliderPressX_ = point.x;
}

void AppController::onRecordSliderReleased(lv_event_t* e) {
    lv_obj_t* recordSlider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    const int32_t dragDistance = point.x - s_instance->recordSliderPressX_;
    // Require both a completed drag past the threshold AND real horizontal
    // movement, so a single tap on the right-hand side of the track can't
    // masquerade as a swipe.
    if (lv_slider_get_value(recordSlider) >= kRecordThreshold && dragDistance >= kMinSwipeDistance) {
        onRecordEnd(nullptr);
    }
    lv_slider_set_value(recordSlider, 0, LV_ANIM_ON);
}

void AppController::onEndGameSliderPressed(lv_event_t*) {
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    s_instance->endGameSliderPressX_ = point.x;
}

void AppController::onEndGameSliderReleased(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    const int32_t dragDistance = point.x - s_instance->endGameSliderPressX_;
    if (lv_slider_get_value(slider) >= kRecordThreshold && dragDistance >= kMinSwipeDistance) {
        s_instance->endCurrentGame();
        return;  // Screen was destroyed; don't touch the slider below.
    }
    lv_slider_set_value(slider, 0, LV_ANIM_ON);
}

void AppController::recordEnd(int team1Score, int team2Score) {
    if (!currentGame_->recordEnd(team1Score, team2Score)) return;
    refreshEndsTable();
    storage_.saveInProgress(*currentGame_);
    if (scoreAnnouncer_ != nullptr) {
        scoreAnnouncer_(currentGame_->team1().score, currentGame_->team2().score, false);
    }
    if (currentGame_->hasReachedWinningScore()) {
        activateEndGameSlider();
    }
}

void AppController::recordDeadEnd() {
    if (!currentGame_->recordDeadEnd()) return;
    refreshEndsTable();
    storage_.saveInProgress(*currentGame_);
    if (scoreAnnouncer_ != nullptr) {
        scoreAnnouncer_(currentGame_->team1().score, currentGame_->team2().score, true);
    }
}

void AppController::activateEndGameSlider() {
    awaitingEndConfirmation_ = true;
    endGameSlider_ = slider_;
    lv_slider_set_range(slider_, 0, kRecordSliderMax);
    lv_slider_set_value(slider_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_, lv_palette_darken(LV_PALETTE_GREY, 1), LV_PART_INDICATOR);
    lv_label_set_text(sliderLabel_, "Slide to End Game");
    lv_obj_set_style_text_color(sliderLabel_, lv_color_black(), 0);
    lv_obj_add_event_cb(slider_, onEndGameSliderPressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(slider_, onEndGameSliderReleased, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_flag(recordSlider_, LV_OBJ_FLAG_HIDDEN);
}

void AppController::refreshEndsTable() {
    populateEndsTable(endsTable_, *currentGame_);
    // Update the container's own layout (not just the table's) so its
    // scrollable range reflects the newly added row before we scroll it.
    lv_obj_update_layout(endsTableContainer_);
    lv_obj_scroll_to_y(endsTableContainer_, LV_COORD_MAX, LV_ANIM_OFF);
}

void AppController::populateEndsTable(lv_obj_t* table, const BowlsGame& game) {
    const auto& ends = game.ends();
    const bool hasHandicap = game.team1().handicap != 0 || game.team2().handicap != 0;
    const uint16_t firstEndRow = hasHandicap ? 2 : 1;

    lv_table_set_row_cnt(table, static_cast<uint16_t>(ends.size() + firstEndRow));
    lv_table_set_cell_value(table, 0, 0, "Scr");
    lv_table_set_cell_value(table, 0, 1, "Tot");
    lv_table_set_cell_value(table, 0, 2, "End");
    lv_table_set_cell_value(table, 0, 3, "Scr");
    lv_table_set_cell_value(table, 0, 4, "Tot");

    int homeTotal = game.team1().handicap;
    int awayTotal = game.team2().handicap;
    char buf[8];
    if (hasHandicap) {
        lv_table_set_cell_value(table, 1, 0, "Hcap");
        std::snprintf(buf, sizeof(buf), "%d", homeTotal);
        lv_table_set_cell_value(table, 1, 1, buf);
        lv_table_set_cell_value(table, 1, 2, "-");
        lv_table_set_cell_value(table, 1, 3, "Hcap");
        std::snprintf(buf, sizeof(buf), "%d", awayTotal);
        lv_table_set_cell_value(table, 1, 4, buf);
    }
    for (size_t i = 0; i < ends.size(); ++i) {
        const EndResult& end = ends[i];
        homeTotal += end.team1Score;
        awayTotal += end.team2Score;
        const uint16_t row = static_cast<uint16_t>(i + firstEndRow);

        if (end.team1Score == 0 && end.team2Score == 0) {
            lv_table_set_cell_value(table, row, 0, "-");
            lv_table_set_cell_value(table, row, 3, "-");
        } else if (end.team1Score > 0) {
            std::snprintf(buf, sizeof(buf), "%d", end.team1Score);
            lv_table_set_cell_value(table, row, 0, buf);
            lv_table_set_cell_value(table, row, 3, "-");
        } else {
            lv_table_set_cell_value(table, row, 0, "-");
            std::snprintf(buf, sizeof(buf), "%d", end.team2Score);
            lv_table_set_cell_value(table, row, 3, buf);
        }

        std::snprintf(buf, sizeof(buf), "%d", homeTotal);
        lv_table_set_cell_value(table, row, 1, buf);
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(i + 1));
        lv_table_set_cell_value(table, row, 2, buf);
        std::snprintf(buf, sizeof(buf), "%d", awayTotal);
        lv_table_set_cell_value(table, row, 4, buf);
    }
}

void AppController::onEndsTableDrawPart(lv_event_t* e) {
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_ITEMS || dsc->label_dsc == nullptr) return;

    // Table cell "id" encodes row * col_cnt + col; decode both from it.
    lv_obj_t* table = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const uint16_t colCnt = lv_table_get_col_cnt(table);
    const uint32_t row = dsc->id / colCnt;
    const uint32_t col = dsc->id % colCnt;

    // Score/total columns (0, 1, 3, 4) get the largest available font; the
    // header row and the small end-number column stay at the default size.
    const bool scoreColumn = (col == 0 || col == 1 || col == 3 || col == 4);
    if (scoreColumn && row != 0) {
        dsc->label_dsc->font = &lv_font_montserrat_32;
    }
}

void AppController::onEndsTableLongPressed(lv_event_t* e) {
    lv_obj_t* table = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint16_t row = 0;
    uint16_t col = 0;
    lv_table_get_selected_cell(table, &row, &col);
    const bool hasHandicap = s_instance->currentGame_->team1().handicap != 0 ||
                             s_instance->currentGame_->team2().handicap != 0;
    const uint16_t firstEndRow = hasHandicap ? 2 : 1;
    if (row == LV_TABLE_CELL_NONE || row < firstEndRow) return;
    s_instance->showEndMenu(static_cast<int>(row - firstEndRow));
}

void AppController::showEndMenu(int endIndex) {
    if (endMenuOverlay_ != nullptr) return;
    endMenuIndex_ = endIndex;

    endMenuOverlay_ = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(endMenuOverlay_);
    lv_obj_set_size(endMenuOverlay_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(endMenuOverlay_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(endMenuOverlay_, LV_OPA_50, 0);
    lv_obj_clear_flag(endMenuOverlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(endMenuOverlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(endMenuOverlay_, onEndMenuCancel, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* panel = lv_obj_create(endMenuOverlay_);
    stylePanel(panel);
    lv_obj_set_size(panel, 200, 200);
    lv_obj_center(panel);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* titleLabel = lv_label_create(panel);
    lv_label_set_text_fmt(titleLabel, "End %d", endIndex + 1);
    styleBodyLabel(titleLabel);

    lv_obj_t* editBtn = addButton(panel, "Edit", onEndMenuEdit);
    styleButton(editBtn, 160, 48);

    lv_obj_t* deleteBtn = addButton(panel, "Delete", onEndMenuDelete);
    styleButton(deleteBtn, 160, 48);

    lv_obj_t* cancelBtn = addButton(panel, "Cancel", onEndMenuCancel);
    styleButton(cancelBtn, 160, 48);
}

void AppController::closeEndMenu() {
    if (endMenuOverlay_ != nullptr) {
        lv_obj_del(endMenuOverlay_);
        endMenuOverlay_ = nullptr;
    }
    endMenuIndex_ = -1;
}

void AppController::onEndMenuEdit(lv_event_t*) {
    const int index = s_instance->endMenuIndex_;
    s_instance->closeEndMenu();
    if (index < 0) return;

    const EndResult& end = s_instance->currentGame_->ends()[static_cast<size_t>(index)];
    const int32_t maxPerEnd = s_instance->currentGame_->maxPerEnd();
    int32_t value = maxPerEnd;
    if (end.team1Score > 0) {
        value = maxPerEnd + end.team1Score;
    } else if (end.team2Score > 0) {
        value = maxPerEnd - end.team2Score;
    }

    s_instance->editingEndIndex_ = index;
    lv_slider_set_value(s_instance->slider_, value, LV_ANIM_OFF);
    s_instance->updateSliderLabel(value);
    lv_label_set_text(s_instance->recordSliderLabel_, "Slide to Save Edit");
}

void AppController::onEndMenuDelete(lv_event_t*) {
    const int index = s_instance->endMenuIndex_;
    s_instance->closeEndMenu();
    if (index < 0) return;

    s_instance->currentGame_->removeEnd(static_cast<size_t>(index));
    s_instance->refreshEndsTable();
    s_instance->storage_.saveInProgress(*s_instance->currentGame_);
}

void AppController::onEndMenuCancel(lv_event_t*) {
    s_instance->closeEndMenu();
}

void AppController::endCurrentGame() {
    currentGame_->finish(static_cast<uint32_t>(lv_tick_get() / 1000));
    history_.addGame(*currentGame_);
    storage_.save(history_);
    storage_.clearInProgress();
    currentGame_.reset();
    hasInProgressGame_ = false;
    showMenu();
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

void AppController::showHistoryList() {
    lv_obj_t* screen = createScreen();
    addTitle(screen, "Old Scores");

    lv_obj_t* list = lv_list_create(screen);
    lv_obj_set_size(list, LV_PCT(94), LV_PCT(74));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(list, lv_color_black(), 0);
    lv_obj_set_style_bg_color(list, lv_color_white(), 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    if (hasInProgressGame_ && currentGame_) {
        const BowlsGame& game = *currentGame_;
        char text[64];
        std::snprintf(text, sizeof(text), "In Progress: %s vs %s : %d - %d",
                      game.team1().playerNames.empty() ? "" : game.team1().playerNames[0].c_str(),
                      game.team2().playerNames.empty() ? "" : game.team2().playerNames[0].c_str(),
                      game.team1().score, game.team2().score);
        lv_obj_t* btn = lv_list_add_btn(list, nullptr, text);
        lv_obj_set_height(btn, 64);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(btn, lv_color_black(), 0);
        lv_obj_set_style_bg_color(btn, lv_palette_lighten(LV_PALETTE_ORANGE, 3), 0);
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_ORANGE), LV_STATE_PRESSED);
        lv_obj_add_event_cb(btn, onHistoryItemClicked, LV_EVENT_PRESSED, nullptr);
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(static_cast<intptr_t>(history_.count())));
    }

    for (size_t i = 0; i < history_.count(); ++i) {
        const BowlsGame& game = history_.at(i);
        char text[64];
        std::snprintf(text, sizeof(text), "%s vs %s : %d - %d",
                      game.team1().playerNames.empty() ? "" : game.team1().playerNames[0].c_str(),
                      game.team2().playerNames.empty() ? "" : game.team2().playerNames[0].c_str(),
                      game.team1().score, game.team2().score);
        lv_obj_t* btn = lv_list_add_btn(list, nullptr, text);
        lv_obj_set_height(btn, 64);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(btn, lv_color_black(), 0);
        lv_obj_set_style_bg_color(btn, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_STATE_PRESSED);
        lv_obj_add_event_cb(btn, onHistoryItemClicked, LV_EVENT_PRESSED, nullptr);
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }

    lv_obj_t* backBtn = addButton(screen, "Back", onBackToMenu);
    styleButton(backBtn, 150, kButtonHeight);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppController::onHistoryItemClicked(lv_event_t* e) {
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const size_t index = static_cast<size_t>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));
    s_instance->showHistoryDetail(index);
}

void AppController::showHistoryDetail(size_t index) {
    lv_obj_t* screen = createScreen();
    const bool isInProgress = (index == history_.count());
    const BowlsGame& game = isInProgress ? *currentGame_ : history_.at(index);

    char title[32];
    std::snprintf(title, sizeof(title), "%s Game",
                  game.type() == GameType::Doubles ? "Doubles" : "Singles");
    addTitle(screen, title);

    lv_obj_t* summary = lv_label_create(screen);
    styleBodyLabel(summary);
    lv_label_set_long_mode(summary, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(summary, LV_PCT(90));
    char buf[144];
    std::snprintf(buf, sizeof(buf), "Home: %d\nAway: %d\nEnds played: %d\n%s",
                  game.team1().score, game.team2().score, game.endCount(),
                  isInProgress ? "In progress" : game.resultSummary().c_str());
    lv_label_set_text(summary, buf);
    lv_obj_align(summary, LV_ALIGN_TOP_MID, 0, 50);

    // Scorecard: full end-by-end breakdown, matching the live scoring table.
    lv_obj_t* tableContainer = lv_obj_create(screen);
    lv_obj_set_style_pad_all(tableContainer, 0, 0);
    lv_obj_set_scroll_dir(tableContainer, LV_DIR_VER);
    lv_obj_set_size(tableContainer, LV_PCT(94), 190);
    lv_obj_align(tableContainer, LV_ALIGN_TOP_MID, 0, 150);

    lv_obj_t* table = lv_table_create(tableContainer);
    lv_obj_set_style_text_font(table, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_table_set_col_cnt(table, 5);
    lv_table_set_col_width(table, 0, 70);
    lv_table_set_col_width(table, 1, 86);
    lv_table_set_col_width(table, 2, 28);
    lv_table_set_col_width(table, 3, 70);
    lv_table_set_col_width(table, 4, 86);
    lv_obj_add_event_cb(table, onEndsTableDrawPart, LV_EVENT_DRAW_PART_BEGIN, nullptr);
    populateEndsTable(table, game);

    if (isInProgress) {
        lv_obj_t* continueBtn = addButton(screen, "Continue", onMenuContinue);
        styleButton(continueBtn, kStartButtonWidth, kButtonHeight);
        styleForwardButton(continueBtn);
        lv_obj_align(continueBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

        lv_obj_t* backBtn = addButton(screen, "Back", onMenuHistory);
        styleButton(backBtn, 150, kButtonHeight);
        lv_obj_align(backBtn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    } else {
        lv_obj_t* backBtn = addButton(screen, "Back", onMenuHistory);
        styleButton(backBtn, 150, kButtonHeight);
        lv_obj_align(backBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
    }
}

void AppController::onBackToMenu(lv_event_t*) {
    s_instance->showMenu();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void AppController::onMenuSettings(lv_event_t*) {
    s_instance->showSettings();
}

void AppController::showSettings() {
    lv_obj_t* screen = createScreen();
    addTitle(screen, "Settings");

    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, "Screen Brightness");
    styleBodyLabel(label);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 56);

    settingsBrightnessSlider_ = lv_slider_create(screen);
    lv_slider_set_range(settingsBrightnessSlider_, kMinBrightnessPercent, 100);
    lv_slider_set_value(settingsBrightnessSlider_, brightnessPercent_, LV_ANIM_OFF);
    lv_obj_set_size(settingsBrightnessSlider_, LV_PCT(88), 40);
    lv_obj_align(settingsBrightnessSlider_, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_add_event_cb(settingsBrightnessSlider_, onSettingsBrightnessChanged, LV_EVENT_VALUE_CHANGED,
                        nullptr);

    settingsBrightnessLabel_ = lv_label_create(screen);
    lv_obj_set_style_text_font(settingsBrightnessLabel_, &lv_font_montserrat_28, 0);
    lv_label_set_text_fmt(settingsBrightnessLabel_, "%d%%", brightnessPercent_);
    lv_obj_align(settingsBrightnessLabel_, LV_ALIGN_TOP_MID, 0, 150);

    lv_obj_t* audioLabel = lv_label_create(screen);
    lv_label_set_text(audioLabel, "Score Announcement Volume");
    styleBodyLabel(audioLabel);
    lv_obj_align(audioLabel, LV_ALIGN_TOP_MID, 0, 184);

    settingsAudioVolumeSlider_ = lv_slider_create(screen);
    lv_slider_set_range(settingsAudioVolumeSlider_, 0, 100);
    lv_slider_set_value(settingsAudioVolumeSlider_, audioVolumePercent_, LV_ANIM_OFF);
    lv_obj_set_size(settingsAudioVolumeSlider_, LV_PCT(88), 40);
    lv_obj_align(settingsAudioVolumeSlider_, LV_ALIGN_TOP_MID, 0, 222);
    lv_obj_add_event_cb(settingsAudioVolumeSlider_, onSettingsAudioVolumeChanged,
                        LV_EVENT_VALUE_CHANGED, nullptr);

    settingsAudioVolumeLabel_ = lv_label_create(screen);
    lv_obj_set_style_text_font(settingsAudioVolumeLabel_, &lv_font_montserrat_28, 0);
    lv_label_set_text_fmt(settingsAudioVolumeLabel_, "%d%%", audioVolumePercent_);
    lv_obj_align(settingsAudioVolumeLabel_, LV_ALIGN_TOP_MID, 0, 274);

    lv_obj_t* resetBtn = addButton(screen, "Reset User Data", onSettingsResetRequested);
    styleButton(resetBtn, kMenuButtonWidth, 64);
    lv_obj_set_style_bg_color(resetBtn, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_color(resetBtn, lv_palette_darken(LV_PALETTE_RED, 2), LV_STATE_PRESSED);
    lv_obj_align(resetBtn, LV_ALIGN_BOTTOM_MID, 0, -88);

    lv_obj_t* backBtn = addButton(screen, "Back", onBackToMenu);
    styleButton(backBtn, 150, kButtonHeight);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppController::onSettingsBrightnessChanged(lv_event_t* e) {
    const int32_t value = lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(e)));
    s_instance->brightnessPercent_ = static_cast<int>(value);
    lv_label_set_text_fmt(s_instance->settingsBrightnessLabel_, "%d%%", value);
    s_instance->applyBrightness();
}

void AppController::applyBrightness() {
    pushBrightnessToHardware();
    storage_.saveBrightness(static_cast<uint8_t>((brightnessPercent_ * 255) / 100));
}

void AppController::pushBrightnessToHardware() {
    const uint8_t raw = static_cast<uint8_t>((brightnessPercent_ * 255) / 100);
    if (brightnessSetter_ != nullptr) {
        brightnessSetter_(raw);
    }
}

void AppController::enterDisplaySleep() {
    if (displaySleeping_) return;
    displaySleeping_ = true;
    if (brightnessSetter_ != nullptr) {
        brightnessSetter_(0);
    }
}

void AppController::exitDisplaySleep() {
    if (!displaySleeping_) return;
    displaySleeping_ = false;
    pushBrightnessToHardware();
}

void AppController::onSettingsAudioVolumeChanged(lv_event_t* e) {
    const int32_t value = lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(e)));
    s_instance->audioVolumePercent_ = static_cast<int>(value);
    lv_label_set_text_fmt(s_instance->settingsAudioVolumeLabel_, "%d%%", value);
    s_instance->applyAudioVolume();
}

void AppController::applyAudioVolume() {
    if (audioVolumeSetter_ != nullptr) {
        audioVolumeSetter_(static_cast<uint8_t>(audioVolumePercent_));
    }
    storage_.saveAudioVolume(static_cast<uint8_t>(audioVolumePercent_));
}

void AppController::onSettingsResetRequested(lv_event_t*) {
    s_instance->showResetConfirm();
}

void AppController::showResetConfirm() {
    if (resetConfirmOverlay_ != nullptr) return;

    resetConfirmOverlay_ = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(resetConfirmOverlay_);
    lv_obj_set_size(resetConfirmOverlay_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(resetConfirmOverlay_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(resetConfirmOverlay_, LV_OPA_50, 0);
    lv_obj_clear_flag(resetConfirmOverlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(resetConfirmOverlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(resetConfirmOverlay_, onResetConfirmCancel, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* panel = lv_obj_create(resetConfirmOverlay_);
    stylePanel(panel);
    lv_obj_set_size(panel, 280, 220);
    lv_obj_center(panel);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* label = lv_label_create(panel);
    lv_label_set_text(label, "Delete all saved games\nand reset settings?");
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    styleBodyLabel(label);

    lv_obj_t* confirmBtn = addButton(panel, "Yes, Reset", onResetConfirmYes);
    styleButton(confirmBtn, 220, 56);
    lv_obj_set_style_bg_color(confirmBtn, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_color(confirmBtn, lv_palette_darken(LV_PALETTE_RED, 2), LV_STATE_PRESSED);

    lv_obj_t* cancelBtn = addButton(panel, "Cancel", onResetConfirmCancel);
    styleButton(cancelBtn, 220, 56);
}

void AppController::closeResetConfirm() {
    if (resetConfirmOverlay_ != nullptr) {
        lv_obj_del(resetConfirmOverlay_);
        resetConfirmOverlay_ = nullptr;
    }
}

void AppController::onResetConfirmYes(lv_event_t*) {
    s_instance->closeResetConfirm();
    s_instance->resetUserData();
}

void AppController::onResetConfirmCancel(lv_event_t*) {
    s_instance->closeResetConfirm();
}

void AppController::resetUserData() {
    storage_.resetAll();
    history_.clear();
    currentGame_.reset();
    hasInProgressGame_ = false;
    brightnessPercent_ = kDefaultBrightnessPercent;
    audioVolumePercent_ = kDefaultAudioVolumePercent;
    applyBrightness();
    applyAudioVolume();
    showMenu();
}

}  // namespace bowls
