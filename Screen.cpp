#include <Arduino.h>
#include "Screen.h"
#include "Interlocks.h"

#if ARDUINO_LILYGO_T_DISPLAY_S3 || ARDUINO_LILYGO_T_DONGLE_S3 || ARDUINO_LILYGO_T_HMI
ScreenState screen;

bool screenAcquire() {
  return interlocked_compare_exchange(&screen.lock, 0u, 1u);
}

void screenRelease() {
  interlocked_write(&screen.lock, 0u);
}
#endif
