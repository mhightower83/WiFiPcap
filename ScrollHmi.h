#ifndef SCROLL_H
#define SCROLL_H

#if ARDUINO_LILYGO_T_HMI

// #include "pin_config.h"
#include <TFT_eSPI.h> // Graphics and font library for ST7789 driver chip
#include "Screen.h"

extern TFT_eSPI tft; // = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);   // 135 (+105 dead space) X 240 (+40 +40 Dead space at top and bottom)

#undef ESP_LOGE
#undef ESP_LOGI

// Errors take control of display
// tft.fillScreen(TFT_BLACK);
// tft.setCursor(0, 0);
// tft.printf("%s: " fmt "\n", t, ##__VA_ARGS__);
#define ESP_LOGE(t, fmt, ...) \
  do { \
    ssize_t len = snprintf(NULL, 0, "%s: " fmt "\n", t, ##__VA_ARGS__); \
    if (0 < len) { \
      len++; \
      char buf[len]; \
      snprintf(buf, len, "%s: " fmt "\n", t, ##__VA_ARGS__); \
      scrollStrWrite(buf); \
    } \
  } while(false)

// LCDPost and Infos use display if selected/available
#define ESP_LOGI(t, fmt, ...) \
  do { \
    ssize_t len = snprintf(NULL, 0, "%s: " fmt "\n", t, ##__VA_ARGS__); \
    if (0 < len) { \
      len++; \
      char buf[len]; \
      snprintf(buf, len, "%s: " fmt "\n", t, ##__VA_ARGS__); \
      scrollStrWrite(buf); \
    } \
  } while(false)


#define LCDPost(fmt, ...) \
  do { \
    ssize_t len = snprintf(NULL, 0, "%s: " fmt "\n", TAG, ##__VA_ARGS__); \
    if (0 < len) { \
      len++; \
      char buf[len]; \
      snprintf(buf, len, "%s: " fmt "\n", TAG, ##__VA_ARGS__); \
      scrollStrWrite(buf); \
    } \
  } while(false)
#endif


// setupScroll(TOP_FIXED_AREA, BOT_FIXED_AREA)
// void scrollSetup(uint16_t tfa, uint16_t bfa);
void scrollSetup();
void scrollStrWrite(const char *str);
bool scrollAquire();
void scrollRelease();

#endif
