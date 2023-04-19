/*@create-file:build.opt@

// This comment block is part of using `mkbuildoptglobals.py`.
// To enable its use, follow the description in the README.

// Pick a block to comment and uncomment to select between LilyGo T-Display-S3
// and T-Dongle-S3. For other modules leave both commented out.

// -DARDUINO_LILYGO_T_DISPLAY_S3=1
// // This is needed for TFT_eSPI to find the Hardware matching tft_setup.h file
// // Update this path to point into your Sketch folder ./src/T-Display-S3/
// -I"/home/userid/Arduino/ESPs/WiFiPcap/src/T-Display-S3/"

// -DARDUINO_LILYGO_T_DONGLE_S3=1
// // This is needed for TFT_eSPI to find the Hardware matching tft_setup.h file
// // Update this path to point into your Sketch folder ./src/T-Display-S3/
// -I"/home/userid/Arduino/ESPs/WiFiPcap/src/T-Dongle-S3/"
*/


// If you chose to directly edit the TFT_eSPI library .h file, one of these
// matching your ESP32S3 module will need to be uncommented.
// #define ARDUINO_LILYGO_T_DISPLAY_S3 1
// #define ARDUINO_LILYGO_T_DONGLE_S3  1


#ifndef WIFIPCAP_INO_GLOBALS_H
#define WIFIPCAP_INO_GLOBALS_H


#if ARDUINO_LILYGO_T_DONGLE_S3 && ARDUINO_LILYGO_T_DISPLAY_S3
#error "Both ARDUINO_LILYGO_T_DISPLAY_S3 and ARDUINO_LILYGO_T_DONGLE_S3 are defined"
#endif

#if ARDUINO_LILYGO_T_DONGLE_S3
/*
Using LilyGo T-Dongle-S3

Build with Tools selection:              (defines set)
  Board: "ESP32S3-Dev Module"            -DARDUINO_ESP32S3_DEV=1
  USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
  USB CDC On Boot: "Disabled"            -DARDUINO_USB_CDC_ON_BOOT=0
  Upload Mode: "UART0 / Hardware CDC"
  Flash Size: "16MB (128Mb)"
  PSRAM: "disabled"                      none
*/
#define USE_DRAM_CACHE (32*1024)

// Has hardware support for Micro SDCard, but with the current Arduino Core USB
// is unstable with this option. However, if the Micro SDCard is not plugged
// in, it should be stable.
#define USE_USB_MSC 1

#elif ARDUINO_LILYGO_T_DISPLAY_S3
/*
Using LilyGo T-Display-S3

Build with Tools selection:              (defines set)
  Board: "LilyGo T-DisplayS3"            -DARDUINO_LILYGO_T_DISPLAY_S3=1
  USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
  USB CDC On Boot: "Disabled"            -DARDUINO_USB_CDC_ON_BOOT=0
  Upload Mode: "UART0 / Hardware CDC"
  Flash Size: "16MB (128Mb)"
  PSRAM: "OPI PSRAM"                     -DBOARD_HAS_PSRAM
*/
#define USE_USB_MSC 0

#else
/*
Using ESP32S3 module without PSRAM

Build with Tools selection:              (defines set)
  Board: "ESP32S3-Dev Module"            -DARDUINO_ESP32S3_DEV=1
  USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
  USB CDC On Boot: "Disabled"            -DARDUINO_USB_CDC_ON_BOOT=0
  Upload Mode: "UART0 / Hardware CDC"
  Flash Size: "16MB (128Mb)"
  PSRAM: "disabled"                      none
*/
#endif

// Defaults

// Build support for USB MSC, supports TFCard slot on the LilyGo T-Dongle-S3
#ifndef USE_USB_MSC
#define USE_USB_MSC 0
#endif

// Default pre-filter if never set by python script. Intended to capture a WiFi
// session without all the noise of AP beacons, etc. Othewise, the code defaults
// to receive all packets.
#ifndef USE_WIFIPCAP_FILTER_AP_SESSION
#define USE_WIFIPCAP_FILTER_AP_SESSION 1
#endif

// A cache of WiFi authentication packets is normally kept in PSRAM
// When no PSRAM is available, a small DRAM buffer can be used.
// When both are not available the cache is disabled.
// When both are defined PSRAM assumes Priority
#ifndef !defined(USE_DRAM_CACHE) && !defined(BOARD_HAS_PSRAM)
#define USE_DRAM_CACHE (32*1024)
#else
#ifndef USE_DRAM_CACHE
#define USE_DRAM_CACHE (0)
#endif
#endif

// Drops out informative debug prints
#if 0 == CORE_DEBUG_LEVEL
#ifndef RELEASE_BUILD
#define RELEASE_BUILD (1)
#endif

#else
#ifndef RELEASE_BUILD
#define RELEASE_BUILD (0)
#endif
#endif

#endif // WIFIPCAP_INO_GLOBALS_H
