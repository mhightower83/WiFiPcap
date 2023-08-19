
/*************************************************************
  This sketch implements a simple serial receive terminal
  program for monitoring serial debug messages from another
  board.

  Connect GND to target board GND
  Connect RX line to TX line of target board
  Make sure the target and terminal have the same baud rate
  and serial stettings!

  The sketch works with the ILI9341 TFT 240x320 display and
  the called up libraries.

  The sketch uses the hardware scrolling feature of the
  display. Modification of this sketch may lead to problems
  unless the ILI9341 data sheet has been understood!

  Updated by Bodmer 21/12/16 for TFT_eSPI library:
  https://github.com/Bodmer/TFT_eSPI

  BSD license applies, all text above must be included in any
  redistribution
 *************************************************************/

 // Adapted for LilyGo T-HMI (ST7789 240 x 320 display)
 // A tft_setup.h file needs to be build global
 // #define USER_SETUP_ID 207

 #if ARDUINO_LILYGO_T_HMI

// #include <TFT_eSPI.h> // Hardware-specific library
// #include <SPI.h>
#include <Arduino.h>
#include "pin_config.h"
#include "Interlocks.h"
#include "ScrollHmi.h"

void setupScrollArea(uint16_t tfa, uint16_t bfa);
int scroll_line(uint16_t xLastPos);
void scrollAddress(uint16_t vsp);

// #include <TFT_eSPI.h> // Graphics and font library for ST7789 driver chip
// #include <SPI.h>


// See https://github.com/Bodmer/TFT_eSPI/issues/493#issuecomment-664728250
// for adapting scroll from 240 x 320 to 135 x 240 screens
//
// These offsets are built into the driver to allow 0,0 to match what we see on
// the screen; however, when we step around the driver and write to registers
// directly, as we do with vertical scroll, we need to convert our draw
// coordinates to the Display Chip's realility.
// CGRAM_OFFSET rowstart = 40
//  These are from TFT_eSPI/TFT_Drivers/ST7789_Rotation.h
#if (TFT_WIDTH == 135)
constexpr int rowstart = 40;
constexpr int colstart = 52;
#elif (TFT_HEIGHT == 280)
constexpr int rowstart = 0;
constexpr int colstart = 20;
#elif (TFT_WIDTH == 172)
constexpr int rowstart = 0;
constexpr int colstart = 34;
#elif (TFT_WIDTH == 170)
constexpr int rowstart = 0;
constexpr int colstart = 35;
#else
constexpr int rowstart = 0;
constexpr int colstart = 0;
#endif

/*
  This code is currently organized for tft.setRotation(0) // portrait
*/

// TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
extern TFT_eSPI tft; // = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);   // 135 (+105 dead space) X 240 (+40 +40 Dead space at top and bottom)

// Keep thw title at the top
// #define SCREEN_TOP_FIXED_AREA 16 // Number of lines in top fixed area (lines counted from top of screen)
// #define SCREEN_TOP_FIXED_AREA (TFT_HEIGHT / 2) // Number of lines in top fixed area (lines counted from top of screen)
#ifndef SCREEN_TOP_FIXED_AREA
#error ("SCREEN_TOP_FIXED_AREA define required in Sketch.ino.globals.h")
#endif
#define YMAX 320          // Bottom of screen area - our screen only goes to 240; however, the display chip thinks it is 320.
                          // For scrowl to work TFA + VSA + BFA must = 320

// TFA + VSA + BFA
// VSA - Visable Screen Area 240
// The scrolling area must be a integral multiple of TEXT_HEIGHT
#define TEXT_HEIGHT 16    // Height of text to be printed and scrolled
// Number of lines in bottom fixed area (lines counted from bottom of screen)
// #define BOT_FIXED_AREA (YMAX - TFT_HEIGHT)
// #define BOT_FIXED_AREA 80 // TFT_HEIGHT == 240
#define BOT_FIXED_AREA 0  // TFT_HEIGHT == 320

static_assert(0 == (TFT_HEIGHT / 2) % TEXT_HEIGHT, "TFT: BOT_FIXED_AREA not rounded to TEXT_HEIGHT");

// Max x pixel width
constexpr uint16_t xMax = TFT_WIDTH;
constexpr bool overStrike = false;    // esoteric printing terminal behavior TFT libary does not or bits remove later

// Lines on display that will scroll
// constexpr ssize_t max_lines = (YMAX - SCREEN_TOP_FIXED_AREA - BOT_FIXED_AREA) / TEXT_HEIGHT;
constexpr ssize_t max_lines = (YMAX - SCREEN_TOP_FIXED_AREA - BOT_FIXED_AREA) / TEXT_HEIGHT;

// constexpr ssize_t max_lines = BOT_FIXED_AREA / TEXT_HEIGHT;
static_assert(0 < max_lines, "TFT Scroll: max lines error - check boundary definitions");
static_assert(0 == (YMAX - SCREEN_TOP_FIXED_AREA - BOT_FIXED_AREA) % TEXT_HEIGHT, "TFT Scroll: scrolling area must be a integral multiple of TEXT_HEIGHT");

struct Scroll7789 {
    // The initial y coordinate of the top of the scrolling area
    uint16_t yStart = SCREEN_TOP_FIXED_AREA;

    // The initial y coordinate of the top of the bottom text line
    uint16_t yDraw = YMAX - BOT_FIXED_AREA - TEXT_HEIGHT;

    // Keep track of the drawing x coordinate
    uint16_t xPos = 0;
    uint16_t xPosMax = 0;

    bool delayScroll = false;
    //
    // // A few test variables used during debugging
    // bool change_colour = 1;
    // bool selected = 1;

    // We have to blank the top line each time the display is scrolled, but this takes up to 13 milliseconds
    // for a full width line, meanwhile the serial buffer may be filling... and overflowing
    // We can speed up scrolling of short text lines by just blanking the character we drew
    // We keep all the strings pixel lengths to optimise the speed of the top line blanking
    int blank[max_lines];
} scroll;


void scrollSetup() {

  setupScrollArea(SCREEN_TOP_FIXED_AREA, BOT_FIXED_AREA);

  // Zero the array
  for (byte i = 0; i < max_lines; i++) scroll.blank[i] = 0;
}


static void scrollStrWrite(const char *str) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  while (*str > 0) {
    byte data = *str++;
    if (ESC == data) {
      data = *str++;
      if ('R' == data) {
        tft.setTextColor(TFT_BLACK, TFT_RED);
        data = *str++;
      } else
      if ('L' == data) {
        tft.setTextColor(TFT_WHITE, TFT_BLUE);
        data = *str++;
      } else
      if ('Y' == data) {
        tft.setTextColor(TFT_BLACK, TFT_YELLOW);
        data = *str++;
      } else
      if ('G' == data) {
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
        data = *str++;
      }
    }
    /*
      A chacter printing past xMax pixel will get wrapped to the next line
      NL without CR will perform CR
      NL then CR - CR is effectively a NO-OP
      CR without NL will possition to the beginning of current line
      line scroll is delayed until the printing of the next line
    */
    if (data == '\n') {
      if (scroll.delayScroll) {
        scroll.yDraw = scroll_line(scroll.xPosMax);
        scroll.xPosMax = scroll.xPos = 0;
      } else {
        scroll.delayScroll = true;
      }
    } else if (data == '\r'){
      scroll.xPos = 0;
    } else {
      // Skip unprintable characters
      if (data > 31 && data < 128) {
        // We are about to print. Do we need to scroll
        if (scroll.delayScroll || scroll.xPos >= xMax) {
          scroll.delayScroll = false;
          scroll.yDraw = scroll_line(scroll.xPosMax); // It can take 13ms to scroll and blank 16 pixel lines
          scroll.xPosMax = scroll.xPos = 0;
        }
        if (!overStrike && scroll.xPosMax && 0 == scroll.xPos) {
          // We had a return without a newline. Optional, if not doing overstrike, clear line.
          auto endOfLine = scroll.blank[(max_lines-1 + (scroll.yStart - SCREEN_TOP_FIXED_AREA) / TEXT_HEIGHT) % max_lines];
          tft.fillRect(scroll.xPos, scroll.yDraw, endOfLine, TEXT_HEIGHT, TFT_BLACK);
        }
        uint16_t xPosLast = scroll.xPos;
        scroll.xPos += tft.drawChar(data, scroll.xPos, scroll.yDraw, 2);
        if (scroll.xPos > xMax) {
          // Oops character ran off the screen fixup
          tft.fillRect(xPosLast, scroll.yDraw, xMax, TEXT_HEIGHT, TFT_BLACK);
          scroll.yDraw = scroll_line(xPosLast);
          scroll.xPos = tft.drawChar(data, 0, scroll.yDraw, 2);
          scroll.xPosMax = scroll.xPos;
        }
        scroll.xPosMax = std::max(scroll.xPosMax, scroll.xPos);
      }
    }
  } // while (*str > 0) {
}


void scrollStreamStringWrite(StreamString* ss) {
  // There is a risk with reentrancy.
  // Not an exhaustive solution; however, it should be good enough for a
  // seldom occuring event. We can buffer one concurent call to display a
  // message. I think it is still possible to drop one if printing from both
  // cores and an interrupt occurs and tries to print.
  if (screenAcquire()) {
    scrollStrWrite(ss->c_str());
    delete ss;
    StreamString* msg;
    while ((msg = (StreamString*)interlocked_exchange((volatile void**)&screen.msg, NULL))) {
      scrollStrWrite(msg->c_str());
      delete msg;
    }
    uint32_t dropped = interlocked_read(&screen.dropped);
    // There is a window here for dropped to be incremented before the exchange.
    // It is also possible for a new print to fail while performing this notice;
    // however, this is okay, we just needed to let the user know some messages
    // were lost.
    uint32_t dropped_last = interlocked_exchange(&screen.dropped_last, dropped);
    if (dropped != dropped_last) {
      scrollStrWrite("\x1bR" "\r...\r\n");  // used to signify lost prints
    }
    screenRelease();
  } else {
    interlocked_add(&screen.buffered, 1u);;
    StreamString* dropped = (StreamString*)interlocked_exchange((volatile void**)&screen.msg, ss);
    if (dropped) {
      delete dropped;
      interlocked_add(&screen.dropped, 1u);
    }
  }
}


// ##############################################################################################
// Call this function to scroll the display one text line
// ##############################################################################################
int scroll_line(uint16_t xLastPos) {
  // Save for optomized erase end-of-line
  scroll.blank[(max_lines-1 + (scroll.yStart - SCREEN_TOP_FIXED_AREA) / TEXT_HEIGHT) % max_lines] = xLastPos;

  int yTemp = scroll.yStart; // Store the old yStart, this is where we draw the next line

  // Use the record of line lengths to optimise the rectangle size we need to erase the top line
  tft.fillRect(0, scroll.yStart, scroll.blank[(scroll.yStart - SCREEN_TOP_FIXED_AREA) / TEXT_HEIGHT], TEXT_HEIGHT, TFT_BLACK);

  // Change the top of the scroll area
  scroll.yStart += TEXT_HEIGHT;

  // The value must wrap around as the screen memory is a circular buffer
  if (scroll.yStart >= YMAX - BOT_FIXED_AREA) {
    //+ scroll.yStart = SCREEN_TOP_FIXED_AREA; //Requires TEXT_HEIGHT multiple of ??
    scroll.yStart = SCREEN_TOP_FIXED_AREA + (scroll.yStart - YMAX + BOT_FIXED_AREA);
  }

  // Now we can scroll the display
  scrollAddress(scroll.yStart);
  return  yTemp;
}

// ##############################################################################################
// Setup a portion of the screen for vertical scrolling
// ##############################################################################################
// We are using a hardware feature of the display, so we can only scroll in portrait orientation
void setupScrollArea(uint16_t tfa, uint16_t bfa) {
  if (rowstart) {
    tfa += rowstart;                        // Adjustment to starting 0,0 pixel
    bfa -= rowstart;                        //   of attached screen
  }
  tft.writecommand(ST7789_VSCRDEF);       // Vertical scroll definition
  tft.writedata(tfa >> 8);                // Top Fixed Area line count
  tft.writedata(tfa);
  tft.writedata((YMAX - tfa - bfa) >> 8); // Vertical Scrolling Area line count
  tft.writedata(YMAX - tfa - bfa);
  tft.writedata(bfa >> 8);                // Bottom Fixed Area line count
  tft.writedata(bfa);
}

// ##############################################################################################
// Setup the vertical scrolling start address pointer
// ##############################################################################################
void scrollAddress(uint16_t vsp) {
  vsp += rowstart;
  tft.writecommand(ST7789_VSCRSADD); // Vertical scrolling pointer
  tft.writedata(vsp >> 8);
  tft.writedata(vsp);
}

#endif // #if ARDUINO_LILYGO_T_HMI
