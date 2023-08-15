#include <Arduino.h>
#include "Screen.h"
#include "Interlocks.h"

#if ARDUINO_LILYGO_T_DISPLAY_S3 || ARDUINO_LILYGO_T_DONGLE_S3 || ARDUINO_LILYGO_T_HMI
ScreenState screen;

bool screenAcquire() {
  bool ok = interlocked_compare_exchange(&screen.lock, 0u, 1u);
  if (! ok) screen.locked_count++;
  return ok;
}

void screenRelease() {
  interlocked_write(&screen.lock, 0u);
}
#endif
