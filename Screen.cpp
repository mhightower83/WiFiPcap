#include <Arduino.h>
#include "Screen.h"
#include "Interlocks.h"

ScreenState screen;

bool screenAcquire() {
  bool ok = interlocked_compare_exchange(&screen.lock, 0u, 1u);
  if (! ok) screen.locked_count++;
  return ok;
}

void screenRelease() {
  interlocked_write(&screen.lock, 0u);
}
