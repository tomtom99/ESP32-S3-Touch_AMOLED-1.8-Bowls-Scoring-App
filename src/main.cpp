// Crown Green Bowls Scoring App
// Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.8
//
// This firmware shows a touch-driven menu to start a new singles/doubles
// game or review previously played games. See src/core for the
// hardware-independent scoring rules and src/ui for the LVGL screens.
//
// NOTE: The Arduino_ESP32QSPI/Arduino_CO5300 constructor arguments below
// match the "GFX Library for Arduino" API at the time of writing, but
// display driver constructors occasionally change signature between
// releases. If your installed library version fails to compile here,
// check that library's Waveshare AMOLED example for the current
// constructor signature and update this block accordingly - the rest of
// the app (src/core, src/ui) does not depend on these details.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <lvgl.h>

#include "config/BoardPins.h"
#include "hal/BatteryMonitor.h"
#include "hal/ScoreAnnouncer.h"
#include "hal/TouchDriver.h"
#include "storage/FlashGameStorage.h"
#include "ui/AppController.h"

namespace {

// --- Display -----------------------------------------------------------
Arduino_DataBus* g_bus = new Arduino_ESP32QSPI(
    LCD_CS /* CS */, LCD_SCLK /* SCK */, LCD_SDIO0 /* D0 */, LCD_SDIO1 /* D1 */,
    LCD_SDIO2 /* D2 */, LCD_SDIO3 /* D3 */);
Arduino_CO5300* g_gfx = new Arduino_CO5300(g_bus, -1 /* RST */, 0 /* rotation */,
                                           false /* IPS */, LCD_WIDTH, LCD_HEIGHT);

void setDisplayBrightness(uint8_t brightness) {
    g_gfx->setBrightness(brightness);
}

// --- Touch ---------------------------------------------------------------
bowls::TouchDriver g_touch(TOUCH_IIC_SDA, TOUCH_IIC_SCL, TOUCH_INT);

// --- Battery (onboard AXP2101 PMU, shares the touch/expander I2C bus) ------
bowls::BatteryMonitor g_battery;
bowls::ScoreAnnouncer g_scoreAnnouncer;

// Defined further below alongside storage; forward-declared here so the
// touch/PWR-button callbacks above it can check the display sleep state.
bowls::AppController* g_app = nullptr;

int readBatteryPercent() {
    return g_battery.readPercent();
}

void announceScore(int homeScore, int awayScore, bool deadEnd) {
    g_scoreAnnouncer.announceScore(homeScore, awayScore, deadEnd);
}

void setAudioVolume(uint8_t volumePercent) {
    g_scoreAnnouncer.setVolumePercent(volumePercent);
}

// --- LVGL ------------------------------------------------------------------
constexpr uint32_t kDrawBufLines = 40;
constexpr uint8_t kIoExpanderAddress = 0x20;
constexpr uint8_t kIoExpanderOutputRegister = 0x01;
constexpr uint8_t kIoExpanderConfigRegister = 0x03;
constexpr uint8_t kBoardEnablePinsMask = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 6);
constexpr uint8_t kBoardEnablePinsAsOutputs = static_cast<uint8_t>(~kBoardEnablePinsMask);

lv_disp_draw_buf_t g_drawBuf;
lv_color_t g_buf1[LCD_WIDTH * kDrawBufLines];
lv_disp_drv_t g_dispDrv;
lv_indev_drv_t g_indevDrv;

bool writeIoExpanderRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(kIoExpanderAddress);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool initializeBoardPeripherals() {
    if (!writeIoExpanderRegister(kIoExpanderOutputRegister, 0x00)) {
        return false;
    }
    if (!writeIoExpanderRegister(kIoExpanderConfigRegister, kBoardEnablePinsAsOutputs)) {
        return false;
    }

    delay(20);
    return writeIoExpanderRegister(kIoExpanderOutputRegister, kBoardEnablePinsMask);
}

void lvglFlushCallback(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* colorP) {
    const uint32_t w = area->x2 - area->x1 + 1;
    const uint32_t h = area->y2 - area->y1 + 1;
    g_gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t*>(colorP), w, h);
    lv_disp_flush_ready(drv);
}

void lvglTouchReadCallback(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    static int16_t lastX = 0;
    static int16_t lastY = 0;
    int16_t x = 0;
    int16_t y = 0;
    // Ignore touches while the display is asleep so nothing behind the
    // dark screen reacts before the user wakes it back up.
    if (g_app != nullptr && g_app->isDisplaySleeping()) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = lastX;
        data->point.y = lastY;
        return;
    }
    if (g_touch.read(x, y)) {
        lastX = x;
        lastY = y;
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = lastX;
        data->point.y = lastY;
    }
}

// --- PWR button (read via the shared I/O expander) --------------------------
// The board's PWR button is wired to the TCA9554 I/O expander input port
// rather than a plain ESP32 GPIO (per Waveshare's docs: "the action can be
// judged by the high and low levels of the EXIO4 detection button... high
// level is pressed"). A 6+ second hold powers the board off in hardware;
// short presses are free for us to use here to toggle a software sleep mode
// (dims the backlight and ignores touch) with two double-presses.
constexpr uint8_t kIoExpanderInputRegister = 0x00;
constexpr uint8_t kPwrButtonBit = 1U << 3;  // EXIO4 (1-indexed) -> bit 3
constexpr uint32_t kPwrDoublePressWindowMs = 500;
constexpr uint32_t kPwrDebounceMs = 40;

bool readPwrButtonPressed() {
    Wire.beginTransmission(kIoExpanderAddress);
    Wire.write(kIoExpanderInputRegister);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<int>(kIoExpanderAddress), 1) != 1) return false;
    const uint8_t value = Wire.read();
    return (value & kPwrButtonBit) != 0;
}

void pollPwrButton() {
    static bool lastPressed = false;
    static uint32_t lastChangeMs = 0;
    static uint32_t lastPressMs = 0;
    static uint8_t pressCount = 0;

    const uint32_t now = millis();
    if ((now - lastChangeMs) < kPwrDebounceMs) return;

    const bool pressed = readPwrButtonPressed();
    if (pressed == lastPressed) return;
    lastChangeMs = now;
    lastPressed = pressed;
    if (!pressed) return;  // only act on the press (rising) edge

    pressCount = (now - lastPressMs <= kPwrDoublePressWindowMs) ? pressCount + 1 : 1;
    lastPressMs = now;
    if (pressCount < 2) return;
    pressCount = 0;

    if (g_app == nullptr) return;
    if (g_app->isDisplaySleeping()) {
        g_app->exitDisplaySleep();
    } else {
        g_app->enterDisplaySleep();
    }
}

// --- Storage + app state ---------------------------------------------------
bowls::FlashGameStorage g_storage;

}  // namespace

void setup() {
    Serial.begin(115200);

    Wire.begin(TOUCH_IIC_SDA, TOUCH_IIC_SCL);
    Wire.setClock(400000);
    if (!initializeBoardPeripherals()) {
        Serial.println("Board warning: failed to initialize the I/O expander.");
    }

    g_gfx->begin();
    g_gfx->fillScreen(BLACK);

    g_touch.begin();

    if (!g_battery.begin(Wire, TOUCH_IIC_SDA, TOUCH_IIC_SCL)) {
        Serial.println("Battery warning: failed to initialize the AXP2101 PMU.");
    }
    if (!g_scoreAnnouncer.begin(Wire, TOUCH_IIC_SDA, TOUCH_IIC_SCL)) {
        Serial.println("Audio warning: failed to initialize the ES8311 codec.");
    }

    g_storage.begin();

    lv_init();
    lv_disp_draw_buf_init(&g_drawBuf, g_buf1, nullptr, LCD_WIDTH * kDrawBufLines);

    lv_disp_drv_init(&g_dispDrv);
    g_dispDrv.hor_res = LCD_WIDTH;
    g_dispDrv.ver_res = LCD_HEIGHT;
    g_dispDrv.flush_cb = lvglFlushCallback;
    g_dispDrv.draw_buf = &g_drawBuf;
    lv_disp_drv_register(&g_dispDrv);

    lv_indev_drv_init(&g_indevDrv);
    g_indevDrv.type = LV_INDEV_TYPE_POINTER;
    g_indevDrv.read_cb = lvglTouchReadCallback;
    lv_indev_drv_register(&g_indevDrv);

    static bowls::AppController app(g_storage);
    g_app = &app;
    g_app->setBrightnessSetter(setDisplayBrightness);
    g_app->setBatteryPercentGetter(readBatteryPercent);
    g_app->setScoreAnnouncer(announceScore);
    g_app->setAudioVolumeSetter(setAudioVolume);
    g_app->begin();
}

void loop() {
    pollPwrButton();
    lv_timer_handler();
    delay(5);
}
