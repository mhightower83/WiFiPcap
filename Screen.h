#ifndef SCREEN_H_
#define SCREEN_H_

#if ARDUINO_LILYGO_T_DISPLAY_S3 || ARDUINO_LILYGO_T_DONGLE_S3 || ARDUINO_LILYGO_T_HMI
////////////////////////////////////////////////////////////////////////////////
// LCD Log Info and Error to Display
//
#include <TFT_eSPI.h>
#include "src/Free_Fonts.h"
#include <StreamString.h>

extern TFT_eSPI tft;
struct ScreenState {
  volatile uint32_t lock;
  volatile uint32_t dropped;
  volatile uint32_t dropped_last;
  volatile uint32_t buffered;
  volatile StreamString* msg;
  uint32_t saver_time;
  uint32_t dim_time;
  int32_t  dim;
  int32_t  height;
  int32_t  top_area;
  bool     on;
  bool     refresh = true;
  uint32_t select = 0;
};
extern ScreenState screen;

bool screenAcquire();
void screenRelease();
static inline void screenInit(const int height, const int topArea) {
  screen.on = true;
  screen.lock = 1u;       // start in locked state
  screen.dropped_last =
  screen.dropped =
  screen.buffered = 0;
  screen.height = height;
  screen.top_area = topArea; // also is the pixel line address of 2nd area
}


#if ARDUINO_LILYGO_T_DISPLAY_S3 || ARDUINO_LILYGO_T_DONGLE_S3
void refreshScreen();
void toggleScreen(void);
void selectScreen(const size_t select);
#endif

#undef ESP_LOGE
#undef ESP_LOGI



#if ARDUINO_LILYGO_T_DONGLE_S3
// Errors take control of display
#define ESP_LOGE(t, fmt, ...) \
  do { \
    selectScreen(1); \
    tft.fillScreen(TFT_BLACK); \
    tft.setCursor(0, 0); \
    tft.printf("%s: " fmt "\n", t, ##__VA_ARGS__); \
  } while(false)

// LCDPost and Infos use display if selected/available
#define ESP_LOGI(t, fmt, ...) \
  if (1 == screen.select) { \
    tft.fillScreen(TFT_BLACK); \
    tft.setCursor(0, 0); \
    tft.printf("%s: " fmt "\n", t, ##__VA_ARGS__); \
  }

#define LCDPost(fmt, ...) \
  if (1 == screen.select) { \
    tft.fillScreen(TFT_BLACK); \
    tft.setCursor(0, 0); \
    tft.printf("%s: " fmt "\n", TAG, ##__VA_ARGS__); \
  }

#elif ARDUINO_LILYGO_T_HMI
// On LilyGo T-TMI, we have enough screen area to split the screen in half,
// top for statistics and bottom for logging.
#include "ScrollHmi.h"

#elif ARDUINO_LILYGO_T_DISPLAY_S3
// On LilyGo T-Display-S3, Use Serial port connector next to USB-C Jack
#define ESP_LOGE(t, fmt, ...) HWSerial.printf("%s: " fmt "\n", t, ##__VA_ARGS__)
#define ESP_LOGI(t, fmt, ...) HWSerial.printf("%s: " fmt "\n", t, ##__VA_ARGS__)
#define LCDPost(fmt, ...) HWSerial.printf("%s: " fmt "\n", TAG, ##__VA_ARGS__)
#endif


#else
static inline void refreshScreen(void) {}
static inline void selectScreen([[maybe_unused]] const size_t select) {}
static inline void toggleScreen(void) {}
static inline bool screenAcquire(void) { return true; }
static inline void scrollRelease(void) {}
#define LCDPost(fmt, ...)
#endif // #if ARDUINO_LILYGO_T_DISPLAY_S3 || ARDUINO_LILYGO_T_DONGLE_S3 || ARDUINO_LILYGO_T_HMI


#endif
