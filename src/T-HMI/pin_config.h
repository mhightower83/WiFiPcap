#pragma once

#define PWR_EN_PIN  (10)            // 5V BAT Power EN, V3 EN for ST7789V
#define PWR_ON_PIN  (14)            // 5V Power On - VBUS|DC-IN
#define BAT_ADC_PIN (5)
#define BUTTON1_PIN (0)
#define BUTTON2_PIN (21)

#define PIN_POWER_ON                 PWR_ON_PIN
#define PIN_BAT_VOLT                 BAT_ADC_PIN
#define PIN_BUTTON_1                 BUTTON1_PIN
#define PIN_BUTTON_2                 BUTTON2_PIN


// lcd
#define LCD_DATA0_PIN (48)
#define LCD_DATA1_PIN (47)
#define LCD_DATA2_PIN (39)
#define LCD_DATA3_PIN (40)
#define LCD_DATA4_PIN (41)
#define LCD_DATA5_PIN (42)
#define LCD_DATA6_PIN (45)
#define LCD_DATA7_PIN (46)
#define PCLK_PIN      (8)
#define CS_PIN        (6)
#define DC_PIN        (7)
#define RST_PIN       (-1)
#define BK_LIGHT_PIN  (38)

#define PIN_LCD_BL                   BK_LIGHT_PIN
#define PIN_LCD_BL_ON                HIGH


// touch screen
#define TOUCHSCREEN_SCLK_PIN (1)
#define TOUCHSCREEN_MISO_PIN (4)
#define TOUCHSCREEN_MOSI_PIN (3)
#define TOUCHSCREEN_CS_PIN   (2)
#define TOUCHSCREEN_IRQ_PIN  (9)

// sd card
#define SD_MISO_PIN (13)
#define SD_MOSI_PIN (11)
#define SD_SCLK_PIN (12)

#define SDIO_DATA0_PIN (13)
#define SDIO_CMD_PIN   (11)
#define SDIO_SCLK_PIN  (12)
