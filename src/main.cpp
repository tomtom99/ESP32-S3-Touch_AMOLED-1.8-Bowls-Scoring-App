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
#include "hal/TouchDriver.h"
#include "storage/FlashGameStorage.h"
#include "ui/AppController.h"

namespace {

// --- Display -----------------------------------------------------------
Arduino_DataBus* g_bus = new Arduino_ESP32QSPI(
    LCD_CS /* CS */, LCD_SCLK /* SCK */, LCD_SDIO0 /* D0 */, LCD_SDIO1 /* D1 */,
    LCD_SDIO2 /* D2 */, LCD_SDIO3 /* D3 */);
Arduino_GFX* g_gfx = new Arduino_CO5300(g_bus, -1 /* RST */, 0 /* rotation */,
                                        false /* IPS */, LCD_WIDTH, LCD_HEIGHT);

// --- Touch ---------------------------------------------------------------
bowls::TouchDriver g_touch(TOUCH_IIC_SDA, TOUCH_IIC_SCL, TOUCH_INT);

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

// --- Storage + app state ---------------------------------------------------
bowls::FlashGameStorage g_storage;
bowls::AppController* g_app = nullptr;

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
    g_app->begin();
}

void loop() {
    lv_timer_handler();
    delay(5);
}
