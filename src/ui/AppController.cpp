#include "ui/AppController.h"

#include <cstdio>

namespace bowls {

namespace {

constexpr lv_coord_t kButtonWidth = 150;
constexpr lv_coord_t kButtonHeight = 56;
constexpr lv_coord_t kMenuButtonWidth = 230;
constexpr lv_coord_t kSetupButtonWidth = 160;
constexpr lv_coord_t kSetupButtonHeight = 76;
constexpr lv_coord_t kStartButtonWidth = 190;
constexpr lv_coord_t kStartButtonHeight = 80;

// "Slide to record"/"Swipe to end game" controls only count as a deliberate
// swipe (rather than a stray tap landing far enough along the track to look
// like a completed drag) once the touch point has moved at least this many
// pixels horizontally between press and release.
constexpr int32_t kMinSwipeDistance = 60;

// "Slide to record" control: dragging the knob past this fraction of the
// track confirms the action; releasing early snaps the knob back to zero.
constexpr int32_t kRecordSliderMax = 100;
constexpr int32_t kRecordThreshold = 85;

void styleScreen(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(obj, 12, 0);
}

void stylePanel(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(obj, 8, 0);
    lv_obj_set_style_pad_row(obj, 8, 0);
    lv_obj_set_style_pad_column(obj, 8, 0);
}

void styleButton(lv_obj_t* btn, lv_coord_t width = kButtonWidth, lv_coord_t height = kButtonHeight) {
    lv_obj_set_size(btn, width, height);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_pad_all(btn, 8, 0);
    lv_obj_set_style_text_font(btn, &lv_font_montserrat_20, 0);
}

void styleBodyLabel(lv_obj_t* label) {
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
}

// Fills buf with the display text for a given slider position, where
// `center` is the slider value that represents a dead end (i.e. the value
// halfway along the track, equal to the game's maxPerEnd()).
void sliderValueText(char* buf, size_t bufSize, int32_t value, int32_t center) {
    const int32_t diff = value - center;
    if (diff == 0) {
        std::snprintf(buf, bufSize, "DEAD END");
    } else {
        std::snprintf(buf, bufSize, "%ld %s", static_cast<long>(diff > 0 ? diff : -diff),
                      diff > 0 ? "UP" : "DOWN");
    }
}

// Colour-codes the slider label so the direction is obvious at a glance.
lv_color_t sliderValueColor(int32_t value, int32_t center) {
    if (value > center) return lv_palette_main(LV_PALETTE_GREEN);
    if (value < center) return lv_palette_main(LV_PALETTE_RED);
    return lv_palette_main(LV_PALETTE_GREY);
}

}  // namespace

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
    styleScreen(screen);
    lv_scr_load(screen);
    return screen;
}

lv_obj_t* AppController::addTitle(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
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
    addTitle(screen, "Crown Green Bowls");

    lv_obj_t* newGameBtn = addButton(screen, "New Game", onMenuNewGame);
    styleButton(newGameBtn, kMenuButtonWidth, 64);
    lv_obj_align(newGameBtn, LV_ALIGN_CENTER, 0, -42);

    lv_obj_t* historyBtn = addButton(screen, "View Old Scores", onMenuHistory);
    styleButton(historyBtn, kMenuButtonWidth, 64);
    lv_obj_align(historyBtn, LV_ALIGN_CENTER, 0, 42);
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

void AppController::showNewGameSetup() {
    pendingType_ = GameType::Singles;
    lv_obj_t* screen = createScreen();
    addTitle(screen, "New Game");

    lv_obj_t* subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Home vs Away");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_20, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 46);

    lv_obj_t* typeRow = lv_obj_create(screen);
    stylePanel(typeRow);
    lv_obj_set_size(typeRow, LV_PCT(94), 100);
    lv_obj_align(typeRow, LV_ALIGN_CENTER, 0, -24);
    lv_obj_set_flex_flow(typeRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(typeRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* singlesBtn = lv_btn_create(typeRow);
    lv_obj_add_flag(singlesBtn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_state(singlesBtn, LV_STATE_CHECKED);
    lv_obj_add_event_cb(singlesBtn, onSetupTypeChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_user_data(singlesBtn, reinterpret_cast<void*>(static_cast<intptr_t>(GameType::Singles)));
    styleButton(singlesBtn, kSetupButtonWidth, kSetupButtonHeight);
    lv_obj_t* singlesLabel = lv_label_create(singlesBtn);
    lv_label_set_text(singlesLabel, "Singles");
    lv_obj_center(singlesLabel);

    lv_obj_t* doublesBtn = lv_btn_create(typeRow);
    lv_obj_add_flag(doublesBtn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(doublesBtn, onSetupTypeChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_user_data(doublesBtn, reinterpret_cast<void*>(static_cast<intptr_t>(GameType::Doubles)));
    styleButton(doublesBtn, kSetupButtonWidth, kSetupButtonHeight);
    lv_obj_t* doublesLabel = lv_label_create(doublesBtn);
    lv_label_set_text(doublesLabel, "Doubles");
    lv_obj_center(doublesLabel);

    lv_obj_t* startBtn = addButton(screen, "Start Game", onSetupStart);
    styleButton(startBtn, kStartButtonWidth, kStartButtonHeight);
    lv_obj_align(startBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    lv_obj_t* backBtn = addButton(screen, "Back", onBackToMenu);
    styleButton(backBtn, 130, kButtonHeight);
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
}

void AppController::onSetupStart(lv_event_t*) {
    s_instance->startNewGame(s_instance->pendingType_);
}

void AppController::startNewGame(GameType type) {
    currentGame_ = std::unique_ptr<BowlsGame>(new BowlsGame(
        type, {"Home"}, {"Away"}, static_cast<uint32_t>(lv_tick_get() / 1000)));
    showScoring();
}

// ---------------------------------------------------------------------------
// Scoring screen
// ---------------------------------------------------------------------------

void AppController::showScoring() {
    lv_obj_t* screen = createScreen();
    editingEndIndex_ = -1;
    const int32_t maxPerEnd = currentGame_->maxPerEnd();

    // Thin "swipe to end game" bar across the top, keeping the rest of the
    // tiny 1.8" screen for the ends table and the scoring slider.
    endGameSlider_ = lv_slider_create(screen);
    lv_slider_set_range(endGameSlider_, 0, kRecordSliderMax);
    lv_slider_set_value(endGameSlider_, 0, LV_ANIM_OFF);
    lv_obj_set_size(endGameSlider_, LV_PCT(100), 32);
    lv_obj_align(endGameSlider_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(endGameSlider_, 16, 0);
    lv_obj_set_style_bg_color(endGameSlider_, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_bg_color(endGameSlider_, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    lv_obj_set_style_radius(endGameSlider_, 16, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(endGameSlider_, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_radius(endGameSlider_, 14, LV_PART_KNOB);
    lv_obj_set_style_pad_all(endGameSlider_, 2, LV_PART_KNOB);
    lv_obj_add_event_cb(endGameSlider_, onEndGameSliderPressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(endGameSlider_, onEndGameSliderReleased, LV_EVENT_RELEASED, nullptr);

    endGameSliderLabel_ = lv_label_create(endGameSlider_);
    lv_label_set_text(endGameSliderLabel_, "Swipe to End Game");
    lv_obj_clear_flag(endGameSliderLabel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(endGameSliderLabel_);

    // Scrollable table of every end played so far, plus running totals.
    // Columns: Home score | Home total | End # | Away score | Away total.
    // Long-pressing a row opens a menu to edit or delete that end.
    endsTableContainer_ = lv_obj_create(screen);
    lv_obj_set_style_pad_all(endsTableContainer_, 0, 0);
    lv_obj_set_scroll_dir(endsTableContainer_, LV_DIR_VER);
    lv_obj_set_size(endsTableContainer_, LV_PCT(100), 210);
    lv_obj_align(endsTableContainer_, LV_ALIGN_TOP_MID, 0, 40);

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
    lv_obj_align(slider_, LV_ALIGN_TOP_MID, 0, 268);

    sliderLabel_ = lv_label_create(screen);
    lv_obj_set_style_text_font(sliderLabel_, &lv_font_montserrat_32, 0);
    lv_obj_align(sliderLabel_, LV_ALIGN_TOP_MID, 0, 320);
    lv_obj_add_event_cb(slider_, onSliderChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    recordSlider_ = lv_slider_create(screen);
    lv_slider_set_range(recordSlider_, 0, kRecordSliderMax);
    lv_slider_set_value(recordSlider_, 0, LV_ANIM_OFF);
    lv_obj_set_size(recordSlider_, LV_PCT(92), 64);
    lv_obj_align(recordSlider_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(recordSlider_, 32, 0);
    lv_obj_set_style_bg_color(recordSlider_, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_bg_color(recordSlider_, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_radius(recordSlider_, 32, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(recordSlider_, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_radius(recordSlider_, 28, LV_PART_KNOB);
    lv_obj_set_style_pad_all(recordSlider_, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(recordSlider_, onRecordSliderPressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(recordSlider_, onRecordSliderReleased, LV_EVENT_RELEASED, nullptr);

    recordSliderLabel_ = lv_label_create(recordSlider_);
    lv_label_set_text(recordSliderLabel_, "Slide to Record End");
    lv_obj_set_style_text_font(recordSliderLabel_, &lv_font_montserrat_20, 0);
    lv_obj_clear_flag(recordSliderLabel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(recordSliderLabel_);

    updateSliderLabel(maxPerEnd);
    refreshEndsTable();
}

void AppController::updateSliderLabel(int32_t value) {
    const int32_t center = currentGame_->maxPerEnd();
    char buf[16];
    sliderValueText(buf, sizeof(buf), value, center);
    lv_label_set_text(sliderLabel_, buf);
    lv_obj_set_style_text_color(sliderLabel_, sliderValueColor(value, center), 0);
}

void AppController::onSliderChanged(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    s_instance->updateSliderLabel(lv_slider_get_value(slider));
}

void AppController::onRecordEnd(lv_event_t*) {
    const int32_t maxPerEnd = s_instance->currentGame_->maxPerEnd();
    const int32_t value = lv_slider_get_value(s_instance->slider_);
    const int32_t diff = value - maxPerEnd;
    const int team1Score = diff > 0 ? static_cast<int>(diff) : 0;
    const int team2Score = diff < 0 ? static_cast<int>(-diff) : 0;

    if (s_instance->editingEndIndex_ >= 0) {
        s_instance->currentGame_->editEnd(static_cast<size_t>(s_instance->editingEndIndex_),
                                          team1Score, team2Score);
        s_instance->editingEndIndex_ = -1;
        lv_label_set_text(s_instance->recordSliderLabel_, "Slide to Record End");
        s_instance->refreshEndsTable();
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
    currentGame_->recordEnd(team1Score, team2Score);
    refreshEndsTable();
}

void AppController::recordDeadEnd() {
    currentGame_->recordDeadEnd();
    refreshEndsTable();
}

void AppController::refreshEndsTable() {
    const BowlsGame& game = *currentGame_;
    const auto& ends = game.ends();

    lv_table_set_row_cnt(endsTable_, static_cast<uint16_t>(ends.size() + 1));
    lv_table_set_cell_value(endsTable_, 0, 0, "Scr");
    lv_table_set_cell_value(endsTable_, 0, 1, "Tot");
    lv_table_set_cell_value(endsTable_, 0, 2, "#");
    lv_table_set_cell_value(endsTable_, 0, 3, "Scr");
    lv_table_set_cell_value(endsTable_, 0, 4, "Tot");

    int homeTotal = 0;
    int awayTotal = 0;
    char buf[8];
    for (size_t i = 0; i < ends.size(); ++i) {
        const EndResult& end = ends[i];
        homeTotal += end.team1Score;
        awayTotal += end.team2Score;
        const uint16_t row = static_cast<uint16_t>(i + 1);

        if (end.team1Score == 0 && end.team2Score == 0) {
            lv_table_set_cell_value(endsTable_, row, 0, "-");
            lv_table_set_cell_value(endsTable_, row, 3, "-");
        } else if (end.team1Score > 0) {
            std::snprintf(buf, sizeof(buf), "%d", end.team1Score);
            lv_table_set_cell_value(endsTable_, row, 0, buf);
            lv_table_set_cell_value(endsTable_, row, 3, "-");
        } else {
            lv_table_set_cell_value(endsTable_, row, 0, "-");
            std::snprintf(buf, sizeof(buf), "%d", end.team2Score);
            lv_table_set_cell_value(endsTable_, row, 3, buf);
        }

        std::snprintf(buf, sizeof(buf), "%d", homeTotal);
        lv_table_set_cell_value(endsTable_, row, 1, buf);
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(row));
        lv_table_set_cell_value(endsTable_, row, 2, buf);
        std::snprintf(buf, sizeof(buf), "%d", awayTotal);
        lv_table_set_cell_value(endsTable_, row, 4, buf);
    }

    lv_obj_update_layout(endsTable_);
    lv_obj_scroll_to_y(endsTableContainer_, LV_COORD_MAX, LV_ANIM_OFF);
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
    if (row == LV_TABLE_CELL_NONE || row == 0) return;  // no cell, or the header row
    s_instance->showEndMenu(static_cast<int>(row) - 1);
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
    lv_obj_set_style_bg_color(deleteBtn, lv_palette_main(LV_PALETTE_RED), 0);

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
}

void AppController::onEndMenuCancel(lv_event_t*) {
    s_instance->closeEndMenu();
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
    lv_obj_set_size(list, LV_PCT(94), LV_PCT(74));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_20, 0);

    for (size_t i = 0; i < history_.count(); ++i) {
        const BowlsGame& game = history_.at(i);
        char text[64];
        std::snprintf(text, sizeof(text), "%s vs %s : %d - %d",
                      game.team1().playerNames.empty() ? "" : game.team1().playerNames[0].c_str(),
                      game.team2().playerNames.empty() ? "" : game.team2().playerNames[0].c_str(),
                      game.team1().score, game.team2().score);
        lv_obj_t* btn = lv_list_add_btn(list, nullptr, text);
        lv_obj_set_height(btn, 56);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_20, 0);
        lv_obj_add_event_cb(btn, onHistoryItemClicked, LV_EVENT_PRESSED, nullptr);
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }

    lv_obj_t* backBtn = addButton(screen, "Back", onBackToMenu);
    styleButton(backBtn, 130, kButtonHeight);
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
    styleBodyLabel(summary);
    lv_label_set_long_mode(summary, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(summary, LV_PCT(90));
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Home: %d\nAway: %d\nEnds played: %d\n%s",
                  game.team1().score, game.team2().score, game.endCount(),
                  game.resultSummary().c_str());
    lv_label_set_text(summary, buf);
    lv_obj_align(summary, LV_ALIGN_TOP_MID, 0, 68);

    lv_obj_t* backBtn = addButton(screen, "Back", onMenuHistory);
    styleButton(backBtn, 130, kButtonHeight);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppController::onBackToMenu(lv_event_t*) {
    s_instance->showMenu();
}

}  // namespace bowls
