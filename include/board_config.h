#pragma once

// ── CYD (ESP32-2432S028) pin map ─────────────────────────────
// TFT pins are defined via build_flags in platformio.ini (TFT_eSPI)

// Touch (XPT2046 resistive touch, separate SPI bus on most CYD boards)
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_CLK  25
#define TOUCH_DIN  32
#define TOUCH_DOUT 39

// Touch calibration (typical CYD values — adjust if touch is off)
#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3700
#define TOUCH_MIN_Y 240
#define TOUCH_MAX_Y 3800

// RGB LED on most CYD boards (active LOW)
#define LED_RED    4
#define LED_GREEN  16
#define LED_BLUE   17

// Display
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
