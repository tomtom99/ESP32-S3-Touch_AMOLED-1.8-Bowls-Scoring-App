# ESP32-S3-Touch_AMOLED-1.8-Bowls-Scoring-App

A crown green bowls scoring app for the [Waveshare ESP32-S3-Touch-AMOLED-1.8](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8) touch-screen board.

## Features

- **Main menu** with two options: start a **New Game** or **View Old Scores**.
- Games are **singles** (2 players, 1 per side) or **doubles** (4 players, 2 per side).
- Each player has 2 bowls, so a side can score up to **2 points per end in singles**
  and up to **4 points per end in doubles** (matching real crown green bowls rules,
  where only the winning side scores in an end).
- Games have no fixed target score - players keep recording ends for as long as
  they like.
- An **End Game** button finishes the current game and saves it to the game
  history, which can be reviewed at any time from the main menu. History is
  persisted to the board's flash storage (LittleFS), so past scores survive a
  reboot.

## Project layout

```
src/
  core/     Hardware-independent game rules (BowlsGame, GameHistory, GameStorage)
  storage/  ESP32 flash (LittleFS/JSON) implementation of GameStorage
  ui/       LVGL touch screens (menu, new game setup, scoring, history)
  hal/      Touch controller driver
  config/   Board pin definitions
  main.cpp  Arduino entry point: hardware + LVGL init
test/
  test_bowls_game/  Unity tests for the core game logic
```

## Building

This project uses [PlatformIO](https://platformio.org/).

```sh
# Build and flash the firmware to the board
pio run -e waveshare-esp32s3-amoled -t upload

# Run the core game-logic unit tests on your host machine (no hardware needed)
pio test -e native
```

The `waveshare-esp32s3-amoled` environment targets the ESP32-S3 with Arduino
framework, [LVGL](https://lvgl.io/) for the UI, and
[GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX) to
drive the QSPI AMOLED panel. Pin assignments for the display, touch
controller and audio codec live in `src/config/BoardPins.h` - if you have an
older (V1) revision of the board, you may need to adjust the touch driver in
`src/hal/TouchDriver.cpp` to match its FT3168 controller instead of the V2
board's CST820.
