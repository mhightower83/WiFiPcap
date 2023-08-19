#ifndef SCROLL_H
#define SCROLL_H

#if ARDUINO_LILYGO_T_HMI

// #include "pin_config.h"
#include <TFT_eSPI.h> // Graphics and font library for ST7789 driver chip
extern TFT_eSPI tft; // = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);   // 135 (+105 dead space) X 240 (+40 +40 Dead space at top and bottom)

#include <StreamString.h>
#include "Screen.h"

constexpr byte ESC = 0x1Bu;

#undef ESP_LOGE
#undef ESP_LOGI

// Red background for Errors, ESC R
#define ESP_LOGE(t, fmt, ...) \
  do { \
    StreamString* log = new StreamString(); \
    log->printf("\x1bR" "%s: " fmt "\n", t, ##__VA_ARGS__); \
    scrollStreamStringWrite(log); \
  } while(false)

// LCDPost and Infos use display if ??selected/available??
// Blue background, ESC L
#define ESP_LOGI(t, fmt, ...) \
  do { \
    StreamString* log = new StreamString(); \
    log->printf("\x1bL" "%s: " fmt "\n", t, ##__VA_ARGS__); \
    scrollStreamStringWrite(log); \
  } while(false)

//
#define LCDPost(fmt, ...) \
  do { \
    StreamString* log = new StreamString(); \
    log->printf("%s: " fmt "\n", TAG, ##__VA_ARGS__); \
    scrollStreamStringWrite(log); \
  } while(false)
#endif


// setupScroll(TOP_FIXED_AREA, BOT_FIXED_AREA)
// void scrollSetup(uint16_t tfa, uint16_t bfa);
void scrollSetup();
void scrollStreamStringWrite(StreamString* ss);
bool scrollAquire();
void scrollRelease();

#endif
