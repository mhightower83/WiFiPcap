#ifndef SCREEN_H_
#define SCREEN_H_

#if ARDUINO_LILYGO_T_DISPLAY_S3 || ARDUINO_LILYGO_T_DONGLE_S3 || ARDUINO_LILYGO_T_HMI
////////////////////////////////////////////////////////////////////////////////
// LCD Log Info and Error to Display
//
#include <TFT_eSPI.h>
#include "src/Free_Fonts.h"

extern TFT_eSPI tft;
struct ScreenState {
  volatile uint32_t lock;
  volatile uint32_t locked_count;
  uint32_t saver_time;
  uint32_t dim_time;
  int32_t  dim;
  bool     on;
  bool     refresh = true;
  uint32_t select = 0;
};
extern ScreenState screen;


struct ScrollLock {
    volatile uint32_t lock;
    volatile uint32_t locked_count;
};

extern ScrollLock scroll_lock;

bool screenAcquire();
void screenRelease();

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
#endif // #if ARDUINO_LILYGO_T_DISPLAY_S3 || ARDUINO_LILYGO_T_DONGLE_S3 || ARDUINO_LILYGO_T_HMI


#endif
