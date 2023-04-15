/*@create-file:build.opt@

// To use `mkbuildoptglobals.py`, you need to update platform.local.txt or you
// can move the globals parts from this comment block to `build_opt.h` minus the
// comments.
// Details about this global comment block can be read here:
// https://github.com/esp8266/Arduino/blob/master/doc/faq/a06-global-build-options.rst

// Pick a block to comment and uncomment to select between LilyGo T-Display-S3
// and T-Dongle-S3. For other modules leave both commented out.


-DARDUINO_LILYGO_T_DISPLAY_S3=1
// This is needed for TFT_eSPI to find the Hardware matching tft_setup.h file
// This must point into you Sketch folder ./src/T-Display-S3/
-I"/home/mhightow/Arduino/ESPs/WiFiPcap/src/T-Display-S3/"
-DUSE_USB_MSC=0


//
// -DARDUINO_LILYGO_T_DONGLE_S3=1
// // This is needed for TFT_eSPI to find the Hardware matching tft_setup.h file
// // This must point into you Sketch folder ./src/T-Dongle-S3/
// -I"/home/mhightow/Arduino/ESPs/WiFiPcap/src/T-Dongle-S3/"
// -DUSE_DRAM_CACHE=1
// // Has hardware support for Micro SDCard, but with the current Arduino Core USB
// // is unstable with  this option. However, if the Micro SDCard is not plugged
// // in, it should be stable.
// -DUSE_USB_MSC=1
// // -DUSE_USB_MSC=0


// Remove development phase informative prints
// -DRELEASE_BUILD=1


// If undefined, starts with "all" packets filter
// Default filter if never set by python script.
-DUSE_WIFIPCAP_FILTER_AP_SESSION=1


// Build testing defines, should be commented out
// -DUSE_DRAM_CACHE=1
// -DUSE_USB_MSC=1
*/

#ifndef WIFIPCAP_INO_GLOBALS_H
#define WIFIPCAP_INO_GLOBALS_H


#if ARDUINO_LILYGO_T_DONGLE_S3 && ARDUINO_LILYGO_T_DISPLAY_S3
#error "Both ARDUINO_LILYGO_T_DISPLAY_S3 and ARDUINO_LILYGO_T_DONGLE_S3 are defined"
#endif

#if ARDUINO_LILYGO_T_DONGLE_S3
/*
Using LilyGo T-Dongle-S3

Build with:
  Board: "ESP32S3-Dev Module"            -DARDUINO_ESP32S3_DEV=1
  USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
  USB CDC On Boot: "Enabled"             -DARDUINO_USB_CDC_ON_BOOT=1
  Upload Mode: "UART0 / Hardware CDC"
*/

#elif ARDUINO_LILYGO_T_DISPLAY_S3
/*
Using LilyGo T-Display-S3

Build with:
  Board: "LilyGo T-DisplayS3"            -DARDUINO_LILYGO_T_DISPLAY_S3=1
  USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
  USB CDC On Boot: "Enabled"             -DARDUINO_USB_CDC_ON_BOOT=1
  Upload Mode: "UART0 / Hardware CDC"
*/
#endif

#endif // WIFIPCAP_INO_GLOBALS_H
