// LilyGo_T_Display_S3
// ST7789 using 8-bit Parallel

/*
  Process
  1) find the USER_SETUP_ID for for LilyGo board of concern
  2) then find the .h file and copy paste into a local tft_setup.h file
  3) Done.
*/
#define USER_SETUP_ID 206
#define DISABLE_ALL_LIBRARY_WARNINGS

#define ST7789_DRIVER
#define INIT_SEQUENCE_3 // Using this initialisation sequence improves the display image

#define CGRAM_OFFSET
#define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
//#define TFT_RGB_ORDER TFT_BGR // Colour order Blue-Green-Red

#define TFT_INVERSION_ON
// #define TFT_INVERSION_OFF

#define TFT_PARALLEL_8_BIT

#define TFT_WIDTH 170
#define TFT_HEIGHT 320

#define TFT_DC 7
#define TFT_RST 5

#define TFT_WR 8
#define TFT_RD 9

#define TFT_D0 39
#define TFT_D1 40
#define TFT_D2 41
#define TFT_D3 42
#define TFT_D4 45
#define TFT_D5 46
#define TFT_D6 47
#define TFT_D7 48

// Let ledcSetup, ... handle Backlight control
#define TFT_BL (-1) // 38
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT
