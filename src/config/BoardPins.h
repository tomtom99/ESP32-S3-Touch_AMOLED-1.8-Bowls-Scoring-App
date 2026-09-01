#pragma once

// Pin definitions for the Waveshare ESP32-S3-Touch-AMOLED-1.8 board.
// Source: Waveshare's official pin_config.h / wiki documentation.
// https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8

// --- Display (QSPI, CO5300/SH8601 AMOLED controller) ------------------------
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 11
#define LCD_CS 12
#define LCD_WIDTH 368
#define LCD_HEIGHT 448

// --- Touch (I2C, CST820/FT3168 controller) ---------------------------------
#define TOUCH_IIC_SDA 15
#define TOUCH_IIC_SCL 14
#define TOUCH_INT 21

// --- Audio (I2S codec) -------------------------------------------------------
#define I2S_MCK_IO 16
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
