#include "ui/AppController.h"

#include <cstdio>

namespace bowls {

namespace {

constexpr lv_coord_t kButtonWidth = 150;
constexpr lv_coord_t kButtonHeight = 56;
constexpr lv_coord_t kMenuButtonWidth = 230;
constexpr lv_coord_t kSmallButtonWidth = 96;
constexpr lv_coord_t kSmallButtonHeight = 40;

// Slider positions, left (0) to right (4). The value chosen determines
// which side scores and by how much for the end just played.
constexpr int32_t kSliderMin = 0;
constexpr int32_t kSliderMax = 4;
constexpr int32_t kSliderDeadEnd = 2;

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

// Returns the display text for a given slider position.
const char* sliderValueText(int32_t value) {
    switch (value) {
        case 0: return "2 DOWN";
        case 1: return "1 DOWN";
        case 2: return "DEAD END";
        case 3: return "1 UP";
        case 4: return "2 UP";
        default: return "";
    }
}

// Colour-codes the slider label so the direction is obvious at a glance.
lv_color_t sliderValueColor(int32_t value) {
    if (value > kSliderDeadEnd) return lv_palette_main(LV_PALETTE_GREEN);
    if (value < kSliderDeadEnd) return lv_palette_main(LV_PALETTE_RED);
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
    lv_obj_set_size(typeRow, LV_PCT(94), 82);
    lv_obj_align(typeRow, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_flex_flow(typeRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(typeRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* singlesBtn = lv_btn_create(typeRow);
    lv_obj_add_flag(singlesBtn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_state(singlesBtn, LV_STATE_CHECKED);
    lv_obj_add_event_cb(singlesBtn, onSetupTypeChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_user_data(singlesBtn, reinterpret_cast<void*>(static_cast<intptr_t>(GameType::Singles)));
    styleButton(singlesBtn, 140, kButtonHeight);
    lv_obj_t* singlesLabel = lv_label_create(singlesBtn);
    lv_label_set_text(singlesLabel, "Singles");
    lv_obj_center(singlesLabel);

    lv_obj_t* doublesBtn = lv_btn_create(typeRow);
    lv_obj_add_flag(doublesBtn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(doublesBtn, onSetupTypeChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_user_data(doublesBtn, reinterpret_cast<void*>(static_cast<intptr_t>(GameType::Doubles)));
    styleButton(doublesBtn, 140, kButtonHeight);
    lv_obj_t* doublesLabel = lv_label_create(doublesBtn);
    lv_label_set_text(doublesLabel, "Doubles");
    lv_obj_center(doublesLabel);

    lv_obj_t* startBtn = addButton(screen, "Start Game", onSetupStart);
    styleButton(startBtn, 150, kButtonHeight);
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

    // Small top bar: Undo (left) and End Game (right), keeping the rest of
    // the tiny 1.8" screen for the ends table and the scoring slider.
    lv_obj_t* undoBtn = addButton(screen, "Undo", onUndoEnd);
    styleButton(undoBtn, kSmallButtonWidth, kSmallButtonHeight);
    lv_obj_align(undoBtn, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* endGameBtn = addButton(screen, "End", onEndGame);
    styleButton(endGameBtn, kSmallButtonWidth, kSmallButtonHeight);
    lv_obj_align(endGameBtn, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Scrollable table of every end played so far, plus running totals.
    // Columns: Home score | Home total | End # | Away score | Away total.
    endsTableContainer_ = lv_obj_create(screen);
    lv_obj_set_style_pad_all(endsTableContainer_, 0, 0);
    lv_obj_set_scroll_dir(endsTableContainer_, LV_DIR_VER);
    lv_obj_set_size(endsTableContainer_, LV_PCT(100), 210);
    lv_obj_align(endsTableContainer_, LV_ALIGN_TOP_MID, 0, 48);

    endsTable_ = lv_table_create(endsTableContainer_);
    lv_obj_set_style_text_font(endsTable_, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_table_set_col_cnt(endsTable_, 5);
    lv_table_set_col_width(endsTable_, 0, 66);
    lv_table_set_col_width(endsTable_, 1, 78);
    lv_table_set_col_width(endsTable_, 2, 52);
    lv_table_set_col_width(endsTable_, 3, 66);
    lv_table_set_col_width(endsTable_, 4, 78);

    // Big, ever-visible slider for choosing the outcome of the current end.
    sliderLabel_ = lv_label_create(screen);
    lv_obj_set_style_text_font(sliderLabel_, &lv_font_montserrat_36, 0);
    lv_obj_align(sliderLabel_, LV_ALIGN_TOP_MID, 0, 268);

    slider_ = lv_slider_create(screen);
    lv_slider_set_range(slider_, kSliderMin, kSliderMax);
    lv_slider_set_value(slider_, kSliderDeadEnd, LV_ANIM_OFF);
    lv_obj_set_size(slider_, LV_PCT(92), 40);
    lv_obj_align(slider_, LV_ALIGN_TOP_MID, 0, 320);
    lv_obj_add_event_cb(slider_, onSliderChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* recordBtn = addButton(screen, "RECORD END", onRecordEnd);
    styleButton(recordBtn, LV_PCT(92), 64);
    lv_obj_align(recordBtn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_t* recordLabel = lv_obj_get_child(recordBtn, 0);
    lv_obj_set_style_text_font(recordLabel, &lv_font_montserrat_28, 0);

    updateSliderLabel(kSliderDeadEnd);
    refreshEndsTable();
}

void AppController::updateSliderLabel(int32_t value) {
    lv_label_set_text(sliderLabel_, sliderValueText(value));
    lv_obj_set_style_text_color(sliderLabel_, sliderValueColor(value), 0);
}

void AppController::onSliderChanged(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    s_instance->updateSliderLabel(lv_slider_get_value(slider));
}

void AppController::onRecordEnd(lv_event_t*) {
    const int32_t value = lv_slider_get_value(s_instance->slider_);
    switch (value) {
        case 0: s_instance->recordEnd(0, 2); break;
        case 1: s_instance->recordEnd(0, 1); break;
        case 2: s_instance->recordDeadEnd(); break;
        case 3: s_instance->recordEnd(1, 0); break;
        case 4: s_instance->recordEnd(2, 0); break;
        default: break;
    }
    lv_slider_set_value(s_instance->slider_, kSliderDeadEnd, LV_ANIM_OFF);
    s_instance->updateSliderLabel(kSliderDeadEnd);
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
    lv_table_set_cell_value(endsTable_, 0, 2, "End");
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

void AppController::onUndoEnd(lv_event_t*) {
    s_instance->currentGame_->undoLastEnd();
    s_instance->refreshEndsTable();
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
