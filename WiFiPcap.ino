/*
  Copyright (C) 2023 - M Hightower

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

////////////////////////////////////////////////////////////////////////////////

  WiFiPcap - Uses the WiFi Promiscuous feature of the ESP32-S3 to capture and
  stream PCAP encapsulated packets to Wireshark through the USB CDC interface.

  I have only tested in a Linux environment.

  Built with Arduino IDE 1.8.18 with ESP32 board addon support.


                        Hardware modules tested

  LilyGo T-Display-S3

    Build with:
      Board: "LilyGo T-DisplayS3"            -DARDUINO_LILYGO_T_DISPLAY_S3=1
      USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
      USB CDC On Boot: "Disabled"            -DARDUINO_USB_CDC_ON_BOOT=0
      Upload Mode: "UART0 / Hardware CDC"
      PSRAM: "OPI PSRAM"                     -DBOARD_HAS_PSRAM


  LilyGo T-Dongle-S3

    Build with:
      Board: "ESP32S3-Dev Module"            -DARDUINO_ESP32S3_DEV=1
      USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
      USB CDC On Boot: "Disabled"            -DARDUINO_USB_CDC_ON_BOOT=0
      Upload Mode: "UART0 / Hardware CDC"
      PSRAM: "Disabled"                      none

  All USB options that require TinyUSB should have "Disabled" selected.

  Wireshark links
    https://wiki.wireshark.org/HowToDecrypt802.11
    https://osqa-ask.wireshark.org/questions/22568/filtering-80211-mac-addresses/
    https://osqa-ask.wireshark.org/questions/6293/fcs-check-is-not-displayed/
*/

// #error "Set -DARDUINO_USB_MODE=0, From Arduino IDE Tools->'USB Mode: 'USB-OTG (TinyUSB)'"
// Maybe need -DARDUINO_USB_CDC_ON_BOOT=0 as well ??

//
// Table of Arduino IDE board USB Selections and their defined values:
//
// USB Mode:                            -DARDUINO_USB_MODE={build.usb_mode}
//   Hardware CDC and JTAG              usb_mode=1
//   USB-OTG (TinyUSB)                  usb_mode=0
//
// USB CDC On Boot:                     -DARDUINO_USB_CDC_ON_BOOT={build.cdc_on_boot}
//   Disabled                           cdc_on_boot=0
//   Enabled                            cdc_on_boot=1
//
// USB Firmware MSC On Boot:            -DARDUINO_USB_MSC_ON_BOOT={build.msc_on_boot}
//   Disabled                           msc_on_boot=0
//   Enabled (Requires USB-OTG Mode)    msc_on_boot=1
//
// USB DFU On Boot:                     -DARDUINO_USB_DFU_ON_BOOT={build.dfu_on_boot}
//   Disabled                           dfu_on_boot=0
//   Enabled (Requires USB-OTG Mode)    dfu_on_boot=1
//
// Upload Mode:                        ?? No -D for build. Maybe it is a json entry.
//   UART0 / Hardware CDC              use_1200bps_touch=false & wait_for_upload_port=false
//   USB-OTG CDC (TinyUSB)             use_1200bps_touch=true  & wait_for_upload_port=true
//   This is a feature of the Arduino IDE. For the version I am using it does not
//   wait long enough for the device to reapear before retrying again and again ...
//   ref. https://arduino.github.io/arduino-cli/0.20/platform-specification/
//
// JTAG Adapter:                       Copies debug files into Sketch directory
//   Disabled                          copy_jtag_files=0
//   Integrated USB JTAG               openocdscript=esp32s3-builtin.cfg, copy_jtag_files=1
//   FTDI Adapter                      openocdscript=esp32s3-ftdi.cfg,    copy_jtag_files=1
//   ESP USB Bridge                    openocdscript=esp32s3-bridge.cfg,  copy_jtag_files=1
//
//

#if ARDUINO_USB_MODE
#pragma message ("\n\nThis sketch works best with the 'USB-OTG (TinyUSB)' option\nSet -DARDUINO_USB_MODE=0, From Arduino IDE Tools->'USB Mode: 'USB-OTG (TinyUSB)'\n\n")
/*
  HWCDC has a few deficiencies.
    * Bug in current Arduino ESP32 - 'Serial.end(); Serial.begin();' crashes.
    * No true host connect/disconnect indication. No DTR state management.
    * DTR tightly linked to HW Reset behavior
    * It is hard to sync ESP32 with the Host side's use of USB CDC.
    * For this Sketch, USB-OTG CDC (TinyUSB) works better.
*/
#endif

#include "WiFiPcap.ino.globals.h"

////////////////////////////////////////////////////////////////////////////////
// Device Module, Peripherals and Configurations
#if ARDUINO_LILYGO_T_DISPLAY_S3
#include "src/T-Display-S3/pin_config.h"
#include "src/T-Display-S3/tft_setup.h"

#define USE_DISPLAY 1
#define FLIP_DISPLAY 1
#define USE_LED_CONTROL 1

/* Please make sure your touch IC model. */
#define TOUCH_MODULES_CST_MUTUAL // This is the correct Touch screen for me
// #define TOUCH_MODULES_CST_SELF

#elif ARDUINO_LILYGO_T_DONGLE_S3
// -DARDUINO_ESP32S3_DEV=1 is usually present
#pragma message ("ARDUINO_LILYGO_T_DONGLE_S3")
#include "src/T-Dongle-S3/pin_config.h"
#include "src/T-Dongle-S3/tft_setup.h"

#define USE_DISPLAY 1
#define FLIP_DISPLAY 1
#define USE_LED_CONTROL 0

#else
#pragma warning("Unfamiliar Hardware. Check ESP32 Board selection and build settings.")
#define USE_DISPLAY 0
#define FLIP_DISPLAY 0
#define USE_LED_CONTROL 0
#endif

// No splash images at this time.
#define USE_SHOW_SPLASH_IMG 0

// Not using preferences at this time.
#define USE_PREFERENCES 0

#include "KConfig.h"

#include <Arduino.h>


#ifndef USE_WIFIPCAP_FILTER_AP_SESSION
#define USE_WIFIPCAP_FILTER_AP_SESSION 0
#endif


#if ARDUINO_LILYGO_T_DISPLAY_S3
#include "src/Free_Fonts.h"
// The ARDUINO_LILYGO_T_DISPLAY_S3 module uses parallel for Display
// Please make sure your touch IC model.
#define TOUCH_MODULES_CST_MUTUAL // This is the correct Touch screen
// #define TOUCH_MODULES_CST_SELF
#include <TouchLib.h>
#include <Wire.h>

#elif ARDUINO_LILYGO_T_DONGLE_S3
#include "src/Free_Fonts.h"
#include <SPI.h>
#include <TFT_eSPI.h>

#else
#endif // ARDUINO_LILYGO_T_DISPLAY_S3


#if defined(PIN_BUTTON_1) || defined(PIN_BUTTON_2)
#include <OneButton.h>
#endif


// TODO: Revisit this. Do we need INTERLOCKED_ ... on some of these volatiles
// Need to review volatile uses - it only addresses Single CPU access
// That may be enough for some accesses.
//
// What I want is missing in Arduino ESP32
// #include <esp_cpu.h>    // INTERLOCKED_COMPARE_EXCHANGE()
// #include <core-macros.h>
// extern "C" bool INTERLOCKED_COMPARE_EXCHANGE(volatile uint32_t *addr, uint32_t compare_value, uint32_t new_value);
// #include "missing.h"

// System
#include <nvs_flash.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include "SerialPcap.h"
#include "WiFiPcap.h"
#include "Interlocks.h"
using namespace std;


#if USE_USB_MSC
#include "usb-msc.h"
#endif


#if USE_PREFERENCES
#include <Preferences.h>
#endif

#if ARDUINO_LILYGO_T_DONGLE_S3 && ARDUINO_LILYGO_T_DISPLAY_S3
#error "Both ARDUINO_LILYGO_T_DISPLAY_S3 and ARDUINO_LILYGO_T_DONGLE_S3 are defined"
#endif

static const char *TAG = "WiFi";


#if RELEASE_BUILD
#undef ESP_LOGI
#define ESP_LOGI(t, fmt, ...)
#endif


// Stuff for Builtin User Output and Input devices
#if USE_DISPLAY
TFT_eSPI tft = TFT_eSPI();
const uint32_t kScreenTimeout = 60000;
const uint32_t kScreenDimAfter = 100;

// ledcSetup
#if USE_LED_CONTROL
const uint32_t kLedBLResolution = 10;   // resolution 1-16 bits,
const uint32_t kLedBLfrequency = 2000;  //freq limits depend on resolution
const uint32_t kLedBLchannnel = 0;      // channels 0-15
constexpr uint32_t kLcdBLMaxLevel = (1 << kLedBLResolution) - 1;
#else
constexpr uint32_t kLcdBLMaxLevel = 255;
#endif

ScreenState screen;

#if ARDUINO_LILYGO_T_DISPLAY_S3
#if defined(TOUCH_MODULES_CST_MUTUAL)
TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS328_SLAVE_ADDRESS, PIN_TOUCH_RES);
#elif defined(TOUCH_MODULES_CST_SELF)
TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);
#endif

#if TOUCH_GET_FORM_INT
bool get_int = false;
#endif
#endif // ARDUINO_LILYGO_T_DISPLAY_S3

// Landscape orientation
constexpr int32_t kMaxWidth  = TFT_HEIGHT;
constexpr int32_t kMaxX      = TFT_HEIGHT - 1;
constexpr int32_t kMaxHeight = TFT_WIDTH;
constexpr int32_t kMaxY      = TFT_WIDTH - 1;
#endif // USE_DISPLAY


#ifdef PIN_BUTTON_1
OneButton button1(PIN_BUTTON_1, true);
#endif
#ifdef PIN_BUTTON_2
OneButton button2(PIN_BUTTON_2, true);
#endif

/*
  Filter selection is now configurable. See "struct WiFiPSession"
  Keep as block comment to document selection options for filter and ctrl_filter.

#if USE_WIFIPCAP_FILTER_AP_SESSION
constexpr wifi_promiscuous_filter_t filter = {
    .filter_mask = // from esp-idf/components/esp_wifi/include/esp_wifi_types.h
    WIFI_PROMIS_FILTER_MASK_MGMT       | // packet type of WIFI_PKT_MGMT
    WIFI_PROMIS_FILTER_MASK_DATA       | // packet type of WIFI_PKT_DATA
    0
};
constexpr wifi_promiscuous_filter_t ctrl_filter = { .filter_mask = 0 };

#else // #elif USE_WIFICAP_FILTER_ALL
constexpr wifi_promiscuous_filter_t filter = {
    .filter_mask = // from esp-idf/components/esp_wifi/include/esp_wifi_types.h
    WIFI_PROMIS_FILTER_MASK_ALL        | // all packets
    // WIFI_PROMIS_FILTER_MASK_MGMT       | // packet type of WIFI_PKT_MGMT
    // WIFI_PROMIS_FILTER_MASK_CTRL       | // packet type of WIFI_PKT_CTRL
    // WIFI_PROMIS_FILTER_MASK_DATA       | // packet type of WIFI_PKT_DATA
    // WIFI_PROMIS_FILTER_MASK_MISC       | // packet type of WIFI_PKT_MISC
    // WIFI_PROMIS_FILTER_MASK_DATA_MPDU  | // filter the MPDU which is a kind of WIFI_PKT_DATA
    // WIFI_PROMIS_FILTER_MASK_DATA_AMPDU | // filter the AMPDU which is a kind of WIFI_PKT_DATA
    // WIFI_PROMIS_FILTER_MASK_FCSFAIL    | // filter the FCS failed packets, do not open it in general
    0
};

constexpr wifi_promiscuous_filter_t ctrl_filter = {
    .filter_mask = // from esp-idf/components/esp_wifi/include/esp_wifi_types.h
    //                                       filter control packets with subtype of
    WIFI_PROMIS_CTRL_FILTER_MASK_ALL      |  // all
    // WIFI_PROMIS_CTRL_FILTER_MASK_WRAPPER  |  // Control Wrapper
    // WIFI_PROMIS_CTRL_FILTER_MASK_BAR      |  // Block Ack Request
    // WIFI_PROMIS_CTRL_FILTER_MASK_BA       |  // Block Ack
    // WIFI_PROMIS_CTRL_FILTER_MASK_PSPOLL   |  // PS-Poll
    // WIFI_PROMIS_CTRL_FILTER_MASK_RTS      |  // RTS
    // WIFI_PROMIS_CTRL_FILTER_MASK_CTS      |  // CTS
    // WIFI_PROMIS_CTRL_FILTER_MASK_ACK      |  // ACK
    // WIFI_PROMIS_CTRL_FILTER_MASK_CFEND    |  // CF-END
    // WIFI_PROMIS_CTRL_FILTER_MASK_CFENDACK |  // CF-END+CF-ACK
    0
};
#endif
*/

constexpr uint32_t kIntervalUpdate = 1000u;

struct WiFiPSession {
    uint32_t channel;
    wifi_promiscuous_filter_t filter;
    wifi_promiscuous_filter_t ctrl_filter;
    uint32_t lastScreenUpdate;
};
WiFiPSession __NOINIT_ATTR ws;

struct ChannelStats {
    uint64_t mgmt  = 0;
    uint64_t ctrl  = 0;
    volatile uint64_t dropped = 0;
    uint64_t full = 0;                  // queue full
    uint64_t data  = 0;
    uint64_t error = 0;
    uint64_t total = 0;
    volatile uint64_t totalBytes = 0;
    uint64_t mgmtSubtype[16] = { 0 };
    uint64_t dataSubtype[16] = { 0 };
} cs[maxChannel];

inline static size_t getChannelIndex() {
    return ws.channel - 1;
}

size_t getChannel() {
    return ws.channel;
}

uint32_t getFilter() {
    return ws.filter.filter_mask;
}

#pragma GCC push_options
#pragma GCC optimize("Ofast")
int not_the_one = 0;

#if 0
#define CHECK_CORE() check_core(__LINE__)

// Debug code
inline static void check_core(int line) {
    uint32_t id = xPortGetCoreID();
    if (0 != id) {
      // are we always running on the same core
      not_the_one = line + id *1000;
    }
}
#else
#define CHECK_CORE()
#endif

/*
  Keep for quick reference
*/
#if 0
    const wifi_pkt_rx_ctrl_t rx_ctrl = (wifi_pkt_rx_ctrl_t)pkt->rx_ctrl;
    rx_ctrl.rssi;         // RSSI of packet. unit: dBm (signed)
    rx_ctrl.rate;         // PHY rate encoding of the packet. Only valid for non HT(11bg) packet
    rx_ctrl.sig_mode;     // 0: non HT(11bg) packet; 1: HT(11n) packet; 3: VHT(11ac) packet
    rx_ctrl.mcs;          // Modulation Coding Scheme. If is HT(11n) packet, shows the modulation, range from 0 to 76(MSC0 ~ MCS76)
    rx_ctrl.cwb;          // Channel Bandwidth of the packet. 0: 20MHz; 1: 40MHz
    rx_ctrl.aggregation;  // Aggregation. 0: MPDU packet; 1: AMPDU packet
    rx_ctrl.stbc;         // Space Time Block Code(STBC). 0: non STBC packet; 1: STBC packet
    rx_ctrl.fec_coding;   // Flag is set for 11n packets which are LDPC
    rx_ctrl.sgi;          // Short Guide Interval(SGI). 0: Long GI; 1: Short GI
    rx_ctrl.ampdu_cnt;
    rx_ctrl.ant;          // antenna number packet was received on. WiFi antenna 0 or 1
    rx_ctrl.noise_floor;  // noise floor of RF Module. unit: dBm (signed)
    rx_ctrl.timestamp;    // localtime packet received - may not be precise - uint32_t localtime in microseconds
    rx_ctrl.channel;      // primary channel on which this packet is received
    rx_ctrl.secondary_channel; // secondary channel on which this packet is received. 0: none; 1: above; 2: below
    rx_ctrl.sig_len;      // packet length including Frame Check Sequence(FCS)
    rx_ctrl.rx_state;     // state of the packet. 0: no error; others: error numbers which are not public
#endif


void wifi_promis_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    const wifi_promiscuous_pkt_t* const pkt = (wifi_promiscuous_pkt_t*)buf;
    const wifi_pkt_rx_ctrl_t rx_ctrl = (wifi_pkt_rx_ctrl_t)pkt->rx_ctrl;
    const WiFiPktHdr* const wh = (WiFiPktHdr*)pkt->payload;

    CHECK_CORE();   // Core 0
    if (0 == rx_ctrl.channel || maxChannel < rx_ctrl.channel) return;
    const size_t i = (rx_ctrl.channel - 1); //getChannelIndex();

    // Collect some statistics
    if (rx_ctrl.rx_state) { // 0: no error; others: unpublished error numbers :(
        cs[i].error++;
    } else
    if (WIFI_PKT_MGMT == type){
        cs[i].mgmt++;
        cs[i].mgmtSubtype[wh->fctl.subtype]++;
    } else
    if (WIFI_PKT_DATA == type){
        cs[i].data++;
        cs[i].dataSubtype[wh->fctl.subtype]++;
    } else
    if (WIFI_PKT_CTRL == type){
          cs[i].ctrl++;
    }
    cs[i].total++;
    cs[i].totalBytes += rx_ctrl.sig_len;

    // Queue a copy of packet for Wireshark
    int ret = serial_pcap_cb(buf, type);
    if (ESP_OK == ret) return;

    if (ESP_ERR_NO_MEM == ret) {
        cs[i].dropped++;
    } else
    if (ESP_ERR_TIMEOUT == ret) {
        cs[i].full++;
        cs[i].dropped++;
    } else
    if (ESP_ERR_INVALID_STATE == ret) {
        cs[i].dropped++;
    }
}
#pragma GCC pop_options

void reset_dropped_count(void) {
    cs[getChannelIndex()].dropped = 0;
}

[[maybe_unused]]
uint32_t get_bps() {
    uint64_t bps = cs[getChannelIndex()].totalBytes;
    cs[getChannelIndex()].totalBytes = 0;
    bps *= 8u;
    bps /= 60u;
    return (bps < UINT32_MAX) ? bps : UINT32_MAX;
}

// Wraps around channel number
inline uint32_t limitChannel(int c) {
    if (1 > c) {
        c = maxChannel;
    } else
    if (maxChannel < c) {
        c = 1;
    }
    return c;
}


static void end_promiscuous() {
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
}

uint32_t begin_promiscuous(void) {
    // Interface must be started and idled when changing channel
    // ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    //? What does ESP_ERROR_CHECK go on non-debug builds?
    end_promiscuous();
    ESP_ERROR_CHECK(esp_wifi_set_channel(ws.channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&ws.filter));
    if (ws.ctrl_filter.filter_mask) {
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous_ctrl_filter(&ws.ctrl_filter));
    }
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_promis_cb));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    refreshScreen();
    return ws.channel;
}

uint32_t begin_promiscuous(uint32_t c, uint32_t filter, uint32_t ctrl_filter) {
    ws.channel = limitChannel(c);

    if (0 == filter && 0 == ctrl_filter) {
        // Keep current filter settings
    } else {
        ws.filter.filter_mask = filter;
        ws.ctrl_filter.filter_mask = ctrl_filter & WIFI_PROMIS_CTRL_FILTER_MASK_ALL;
    }
    return begin_promiscuous();
}

uint32_t begin_promiscuous(uint32_t c) {
    ws.channel = limitChannel(c);
    return begin_promiscuous();
}

[[maybe_unused]]
static uint32_t nextChannel() {
    uint32_t ch = begin_promiscuous(ws.channel + 1);
    refreshScreen();
    return ch;
}

[[maybe_unused]]
static uint32_t prevChannel() {
    uint32_t ch = begin_promiscuous(ws.channel - 1);
    refreshScreen();
    return ch;
}

// From esp-idf/examples/network/simple_sniffer/main/simple_sniffer_example_main.c
// Also referenced https://metalfacesec.github.io/programming/security/2020/01/09/esp32-wifi-sniffer-part-1.html
static void wifi_promis_init() {
    nvs_flash_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
    begin_promiscuous(ws.channel);
}

#if USE_LED_CONTROL
inline void setBL(uint32_t level) {
  if (kLcdBLMaxLevel < level) level = kLcdBLMaxLevel;
  ledcWrite(kLedBLchannnel, level);
}
#else
inline void setBL(uint32_t level) {
#if PIN_LCD_BL
    if (level) {
        level = PIN_LCD_BL_ON;
    } else {
        level = (PIN_LCD_BL_ON) ? LOW : HIGH;
    }
    digitalWrite(PIN_LCD_BL, (level) ? HIGH : LOW);
#endif
}
#endif

#if ARDUINO_USB_MODE
void usbCdcEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    // Task "arduino_usb_events" runs (callbacks) with core affinity tskNO_AFFINITY
    if (event_base == ARDUINO_USB_EVENTS) {
        [[maybe_unused]]
        arduino_usb_event_data_t * data = (arduino_usb_event_data_t*)event_data;
        switch (event_id) {
            case ARDUINO_USB_STARTED_EVENT:
                LCDPost("USB PLUGGED");
                break;
            case ARDUINO_USB_STOPPED_EVENT:
                LCDPost("USB UNPLUGGED");
                break;
            case ARDUINO_USB_SUSPEND_EVENT:
                LCDPost("USB SUSPENDED: remote_wakeup_en: %u", data->suspend.remote_wakeup_en);
                break;
            case ARDUINO_USB_RESUME_EVENT:
                LCDPost("USB RESUMED");
                break;
            default:
                LCDPost("USB UNKNOWN(%d)", event_id);
                break;
        }
    } else
    if (event_base == ARDUINO_HW_CDC_EVENTS) {
        arduino_hw_cdc_event_data_t * data = (arduino_hw_cdc_event_data_t*)event_data;
        switch (event_id){
            case ARDUINO_HW_CDC_CONNECTED_EVENT:
                serial_pcap_notifyDtrRts(true, true);
                LCDPost("HW CDC CONNECTED");
                break;
            case ARDUINO_HW_CDC_BUS_RESET_EVENT:
                serial_pcap_notifyDtrRts(false, false);
                LCDPost("HW CDC BUS RESET");
                break;
            case ARDUINO_HW_CDC_RX_EVENT:
                LCDPost("HW CDC RX [%u]", data->rx.len);
                break;
            case ARDUINO_HW_CDC_TX_EVENT:
                //+ LCDPost("HW CDC TX [%u]", data->tx.len);
                break;
            case ARDUINO_HW_CDC_ANY_EVENT:
                LCDPost("HW CDC ANY EVENT [%u]", event_id);
                break;
            default:
                LCDPost("HW CDC UNKNOWN(%d)", event_id);
                break;
        }
    }
}

#else
void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    CHECK_CORE(); // Called from CORE 1
    // Task "arduino_usb_events" runs (callbacks) with core affinity tskNO_AFFINITY
    if (event_base == ARDUINO_USB_EVENTS) {
        [[maybe_unused]]
        arduino_usb_event_data_t * data = (arduino_usb_event_data_t*)event_data;
        switch (event_id) {
            case ARDUINO_USB_STARTED_EVENT:
                LCDPost("USB PLUGGED");
                break;
            case ARDUINO_USB_STOPPED_EVENT:
                LCDPost("USB UNPLUGGED");
                break;
            case ARDUINO_USB_SUSPEND_EVENT:
                LCDPost("USB SUSPENDED: remote_wakeup_en: %u", data->suspend.remote_wakeup_en);
                break;
            case ARDUINO_USB_RESUME_EVENT:
                LCDPost("USB RESUMED");
                break;
            default:
              break;
        }
    }
}
void usbCdcEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    CHECK_CORE(); // Called from CORE 1
    // Task "arduino_usb_events" runs (callbacks) with core affinity tskNO_AFFINITY
    if (event_base == ARDUINO_USB_EVENTS) {
        [[maybe_unused]]
        arduino_usb_event_data_t * data = (arduino_usb_event_data_t*)event_data;
        switch (event_id) {
            case ARDUINO_USB_STARTED_EVENT:
                LCDPost("USB PLUGGED");
                break;
            case ARDUINO_USB_STOPPED_EVENT:
                LCDPost("USB UNPLUGGED");
                break;
            case ARDUINO_USB_SUSPEND_EVENT:
                LCDPost("USB SUSPENDED: remote_wakeup_en: %u", data->suspend.remote_wakeup_en);
                break;
            case ARDUINO_USB_RESUME_EVENT:
                LCDPost("USB RESUMED");
                break;
            default:
              break;
        }
    } else if (event_base == ARDUINO_USB_CDC_EVENTS) {
        arduino_usb_cdc_event_data_t * data = (arduino_usb_cdc_event_data_t*)event_data;
        switch (event_id){
            case ARDUINO_USB_CDC_CONNECTED_EVENT:
                serial_pcap_notifyDtrRts(true, true);
                LCDPost("CDC CONNECTED");
                break;
            case ARDUINO_USB_CDC_DISCONNECTED_EVENT:
                serial_pcap_notifyDtrRts(false, false);
                LCDPost("CDC DISCONNECTED");
                break;
            case ARDUINO_USB_CDC_LINE_STATE_EVENT:
                serial_pcap_notifyDtrRts(data->line_state.dtr, data->line_state.rts);
                // LCDPost("CDC LINE STATE: dtr: %s, rts: %s", (data->line_state.dtr) ? "True" : "False", (data->line_state.rts) ? "True" : "False");
                LCDPost("CDC LINE STATE: DTR: %c, RTS: %c", (data->line_state.dtr) ? 'T' : 'F', (data->line_state.rts) ? 'T' : 'F');
                break;
            case ARDUINO_USB_CDC_LINE_CODING_EVENT:
                {
                  const char parityChar[] ="NOEMSU";
                  const char *stopBitsChar[4] = { "1", "1.5", "2", "U" }; // 0: 1 stop bit - 1: 1.5 stop bits - 2: 2 stop bits
                  size_t parity = data->line_coding.parity; // 0: None - 1: Odd - 2: Even - 3: Mark - 4: Space
                  size_t stop_bits = data->line_coding.stop_bits; // 0: None - 1: Odd - 2: Even - 3: Mark - 4: Space
                  if (parity > 5) parity = 5; // Unknown
                  if (stop_bits > 3) stop_bits = 3; // Unknown
                  // LCDPost("CDC LINE CODING: bit_rate: %u, data_bits: %u, stop_bits: %u, parity: %u", data->line_coding.bit_rate, data->line_coding.data_bits, data->line_coding.stop_bits, data->line_coding.parity);
                  LCDPost("CDC LINE CODING: bps: %u, %u%c%s", data->line_coding.bit_rate, data->line_coding.data_bits, parityChar[parity] , stopBitsChar[stop_bits]);
                }
                break;
            case ARDUINO_USB_CDC_RX_EVENT:
                LCDPost("CDC RX [%u]", data->rx.len);
                break;
            case ARDUINO_USB_CDC_TX_EVENT:
                // LCDPost("CDC TX"); // too many
                break;
            case ARDUINO_USB_CDC_RX_OVERFLOW_EVENT:
                LCDPost("CDC RX OVF %u bytes", data->rx_overflow.dropped_bytes);
                break;
            default:
                LCDPost("CDC UNKNOWN(%d)", event_id);
                break;
        }
    }
}
#endif

void printMemory(Print& out) {
    out.printf("Heap DRAM\n");
    out.printf(" %-7s %d\n", "Total:", ESP.getHeapSize());
    out.printf(" %-7s %d\n", "Free:",  ESP.getFreeHeap());
    out.printf("\nPSRAM\n");
    out.printf(" %-7s %d\n", "Total:", ESP.getPsramSize());
    out.printf(" %-7s %d\n", "Free:", ESP.getFreePsram());
}

bool is_mem_safe(void) {
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#_CPPv416esp_reset_reasonv
    switch (esp_reset_reason())
    {
      case ESP_RST_POWERON:
      case ESP_RST_BROWNOUT:
      case ESP_RST_DEEPSLEEP:
      case ESP_RST_UNKNOWN:
          return false;

      default:
          break;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
void setup() {
#if ! USE_LED_CONTROL && defined(PIN_LCD_BL)
    digitalWrite(PIN_LCD_BL, LOW); //(PIN_LCD_BL_ON) ? LOW : HIGH);
    pinMode(PIN_LCD_BL, OUTPUT);
    // gpio_hold_dis((gpio_num_t)PIN_LCD_BL);
#endif
#if ARDUINO_LILYGO_T_DISPLAY_S3
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_RES);

    pinMode(PIN_POWER_ON, OUTPUT);  // LCD Display chip
    digitalWrite(PIN_POWER_ON, HIGH);

    pinMode(PIN_TOUCH_RES, OUTPUT);
    digitalWrite(PIN_TOUCH_RES, LOW);
    delay(500);
    digitalWrite(PIN_TOUCH_RES, HIGH);
#endif

    HWSerial.begin(115200);
    HWSerial.printf("\nHWSerial is working!!!\n");

    bool init_custom_filter = true;
    if (is_mem_safe() &&
        ws.channel == limitChannel(ws.channel) &&
        0 == (ws.ctrl_filter.filter_mask & ~WIFI_PROMIS_CTRL_FILTER_MASK_ALL) ) {
        // Use existing settings
        //?? Do we need more validation
        init_custom_filter = false;
    } else {
        // Intialize noinit structure
        ws.channel = limitChannel(CONFIG_WIFIPCAP_CHANNEL);
        ws.filter.filter_mask = (USE_WIFIPCAP_FILTER_AP_SESSION) ?
            (WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA) : 0u;
        ws.ctrl_filter.filter_mask = 0u;
        ws.lastScreenUpdate = 0u;
    }
    ws.lastScreenUpdate =  millis() - kIntervalUpdate;  // Force update event on first loop().

#if ARDUINO_USB_ON_BOOT && !ARDUINO_USB_MODE
    // ARDUINO_USB_ON_BOOT (ARDUINO_USB_CDC_ON_BOOT|ARDUINO_USB_MSC_ON_BOOT|ARDUINO_USB_DFU_ON_BOOT)
    #pragma message("\n  ARDUINO_USB_ON_BOOT defined - USB.begin() has already run.\n  Some USB calls to define/config devices may not take.\n")
#endif

    [[maybe_unused]] esp_err_t err_msc = ESP_OK;

#if ARDUINO_USB_MODE
    #pragma message ("\nExperimental HWCDC\n")
    #if USE_USB_MSC
    #pragma message ("Don't mix HWCDC with USBMSC\n")
    #unset USE_USB_MSC
    #endif
#if ARDUINO_USB_CDC_ON_BOOT
    USBSerial.onEvent(usbCdcEventCallback);
#else
    USBSerial.onEvent(usbCdcEventCallback);
    USBSerial.begin();
#endif

#else
    // Using 100% TinyUSB
#if USE_USB_MSC
    // Interfacing to the SD card in the T-Dongle-S3 plug.
    #pragma message ("Experimental USB_MSC")
    err_msc = sd_init();
    if (ESP_OK == err_msc) {
        if (setupMsc()) {
            // Registered for ARDUINO_USB_EVENTS, ARDUINO_USB_ANY_EVENT,
            USB.onEvent(usbEventCallback);
        } else {
            err_msc = ESP_ERR_INVALID_ARG;
            sd_end();
        }
    }
#endif
#if ARDUINO_USB_CDC_ON_BOOT
    // USB.begin() was previously started we may have missed some events
    // Also, USE_USB_MSC may not work.
    USBSerial.onEvent(usbCdcEventCallback); // late
#else
    // Without USB CDC On Boot: "Enabled" -DARDUINO_USB_CDC_ON_BOOT=1
    // For USBSerial to work, we must call USB.begin().
    // To be clear, this is the prefered build path
    // Registered for ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_ANY_EVENT
    USBSerial.onEvent(usbCdcEventCallback);
    USBSerial.begin();
    USB.begin();
#endif
#endif

    // while(!Serial);  // This works with HWCDC and fails with USBCDC
    USBSerial.printf("\n\nBegin Setup ...\n");

    // display
#if USE_DISPLAY
    // Regarding: "E (1260) gpio: gpio_set_level(226): GPIO output gpio_num error"
    // Ignore it, it has to do with the use of -1 for an unused pin number
    // https://github.com/Bodmer/TFT_eSPI/issues/1908
    // https://github.com/espressif/arduino-esp32/issues/6737
    tft.begin();
    tft.setSwapBytes(true);
    // Screen orientation Landscape
#ifdef FLIP_DISPLAY
    // Flip Screen Vertically
    tft.setRotation(3);
#else
    tft.setRotation(1);
#endif

#if USE_SHOW_SPLASH_IMG
    tft.pushImage(0, 0, 320, 170, (uint16_t *)img_logo);
    // Avoid white screen flash. Give the LCD controller time to finish screen
    // fill before turning on the backlight.
    delay(25);
    #if USE_LED_CONTROL
    // Setup PWM control for LED backlight on channel 0
    ledcAttachPin(PIN_LCD_BL, kLedBLchannnel);
    ledcSetup(kLedBLchannnel, kLedBLfrequency, kLedBLResolution);
    #endif
    setBL(kLcdBLMaxLevel);
    delay(2000);          // Give a moment for eyes to adjust on pushed image
    tft.fillScreen(TFT_BLACK);
#elif USE_DISPLAY
    tft.fillScreen(TFT_BLACK);
    // Avoid white screen flash. Give the LCD controller time to finish screen
    // fill before turning on the backlight.
    delay(25);
    #if USE_LED_CONTROL
    // Setup PWM control for LED backlight on channel 0
    ledcSetup(kLedBLchannnel, kLedBLfrequency, kLedBLResolution);
    ledcAttachPin(PIN_LCD_BL, kLedBLchannnel);
    #endif
    setBL(kLcdBLMaxLevel);
#endif

#if ARDUINO_LILYGO_T_DISPLAY_S3
    Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL);
    if (!touch.init()) {
      USBSerial.println("Touch IC not found");
    }
#if TOUCH_GET_FORM_INT
    attachInterrupt(PIN_TOUCH_INT, [] { get_int = true; }, FALLING);
#endif
#endif

    screen.saver_time =
    screen.dim_time = millis();
    screen.dim = kLcdBLMaxLevel;
    screen.on = true;
#endif //  USE_DISPLAY
    wifi_promis_init();

#ifdef PIN_BUTTON_1
    button1.attachClick([]() { nextChannel(); });
    button1.attachDoubleClick([]() { prevChannel(); });
    button1.attachLongPressStart([]() { toggleScreen(); } );
#endif
#ifdef PIN_BUTTON_2
    button2.attachClick([]() { prevChannel(); });
#endif

#if USE_USB_MSC
    if (ESP_OK != err_msc) {
        ESP_LOGE(TAG, "MSC init failed: %s", esp_err_to_name(err_msc));
    }
#endif

    /*
      Serial for HWCDC Serial is very different from HardwareSerial
      setTxBuffer after begin with HWCDC and before for HardwareSerial
    */
    USBSerial.printf("Start serial pcap\n\n");
    if (ESP_OK != serial_pcap_start(&USBSerial, init_custom_filter)) {
        ESP_LOGE(TAG, "Serial pcap failed to start.");
    }

    delay(100);
    printMemory(HWSerial);
}

void userIO() {
    // Handle User IO
    [[maybe_unused]] uint32_t now = millis();

#ifdef PIN_BUTTON_1
    button1.tick();
#endif
#ifdef PIN_BUTTON_2
    button2.tick();
#endif

#if USE_DISPLAY
    if (now - ws.lastScreenUpdate > kIntervalUpdate) {
        ws.lastScreenUpdate = now;
        updateScreen(ws.channel);
    }
#endif

#if ARDUINO_LILYGO_T_DISPLAY_S3
    // On idle, screen fades to black and turn back on with a touch
    bool touched = false;
#if TOUCH_GET_FORM_INT
    if (get_int) {
        get_int = 0;
        touch.read();
        touched = true;
    }
#else
    if (touch.read()) touched = true;
#endif
    if (touched) {
        if (!screen.on) {
            screen.on = true;
        }
        screen.dim = kLcdBLMaxLevel;
        setBL(screen.dim);
        screen.saver_time = screen.dim_time = now;
    } else {
        if (screen.on) {
            if (now - screen.saver_time > kScreenTimeout) {
                // Fade to off
                if (now - screen.dim_time > kScreenDimAfter) {
                    screen.dim_time = now;
                    screen.dim -= 1;
                    if (0 >= screen.dim) {
                      screen.dim = 0;
                      screen.on = false;
                    }
                    setBL(screen.dim);
                }
            }
        }
    }
#endif //#if ARDUINO_LILYGO_T_DISPLAY_S3
}


#pragma GCC push_options
#pragma GCC optimize("Ofast")

#pragma GCC pop_options

// APP_CORE
void loop() {
    userIO();
    delay(10);
}

#if USE_DISPLAY
inline void refreshScreen(void) {
    screen.refresh = true;
}

void selectScreen(const size_t select) {
    if (select == screen.select) return;

    if (0 == select) {
        screen.select = 0;
        screen.refresh = true;
    } else {
        screen.select = 1;
        tft.setTextFont(FONT2);
        // tft.setFreeFont((FSS7);
        tft.setTextDatum(TL_DATUM);
    }
    tft.fillScreen(TFT_BLACK);
}

void toggleScreen() {
    if (screen.select) {
        selectScreen(0);
    } else {
        selectScreen(1);
    }
}
#endif


#if ARDUINO_LILYGO_T_DISPLAY_S3
void updateScreen(size_t chView) {
    if (0 != screen.select) return;

    size_t i = chView - 1;
    uint32_t bps = get_bps();
    static size_t last_i = SIZE_MAX;
    // const int32_t tweak = -2;
    const uint8_t font = GFXFF;
    const int32_t lines = 7;
    tft.setFreeFont(FSS9);

    int32_t xPos = 0;
    int32_t h = tft.fontHeight(GFXFF);
    int32_t gap = (kMaxHeight - lines * h) / (lines + 1);
    int32_t yPos = (gap + h) / 2;
    int32_t sz = 0;

    tft.setTextColor(TFT_BROWN, TFT_BLACK);

    if (i != last_i) {
        tft.setTextDatum(TL_DATUM);
        tft.setFreeFont(FSS9);
        tft.drawString("Channel", xPos, yPos, font);
        yPos += h + gap;
        tft.drawString("MGMT", xPos, yPos, font);
        yPos += h + gap;
        tft.drawString("CTRL", xPos, yPos, font);
        yPos += h + gap;
        tft.drawString("DATA", xPos, yPos, font);
        yPos += h + gap;
        // tft.drawString("ERROR", xPos, yPos, font);
        tft.drawString("KBPS", xPos, yPos, font);
        yPos += h + gap;
        tft.drawString("Dropped", xPos, yPos, font);
        yPos += h + gap;
        tft.drawString("TOTAL", xPos, yPos, font);
    }

    tft.setTextDatum(TR_DATUM);
    tft.setFreeFont(FSS9);
    const int32_t xStart = kMaxWidth/2;
    xPos = kMaxX;
    yPos = gap;
    if (i != last_i) {
        tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
        tft.drawString(String(chView), xPos, yPos, font);
    }
    yPos += h + gap;
    tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
    tft.drawString(String(cs[i].mgmt), xPos, yPos, font);
    yPos += h + gap;
    tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
    tft.drawString(String(cs[i].ctrl), xPos, yPos, font);
    yPos += h + gap;
    tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
    tft.drawString(String(cs[i].data), xPos, yPos, font);
    yPos += h + gap;
    tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
    // tft.drawString(String(cs[i].error), xPos, yPos, font);
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%04u", bps);
        buf[sizeof(buf)-1] = '\0';
        size_t sz = strlen(buf);
        if (sz < sizeof(buf) - 2) {
            memmove(&buf[sz - 2], &buf[sz - 3], 3);
            buf[sz - 3] = '.';
        }
        tft.drawString(String(buf), xPos, yPos, font);
    }
    yPos += h + gap;
    tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
    tft.drawString(String(cs[i].dropped), xPos, yPos, font);
    yPos += h + gap;
    tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
    tft.drawString(String(cs[i].total), xPos, yPos, font);
    last_i = i;
}

#elif ARDUINO_LILYGO_T_DONGLE_S3
void updateScreen(const size_t chView) {
    if (0 != screen.select) return;

    size_t i = chView - 1;
    uint32_t bps = get_bps();
    const uint8_t font = GFXFF;
    const int32_t lines = 3;
    tft.setFreeFont(FSS9);

    int32_t xPos = 0;
    int32_t h = tft.fontHeight(GFXFF);
    int32_t gap = (kMaxHeight - lines * h) / lines;

    // Put lines dropped via rounding at top
    #if 0
    ssize_t top = kMaxHeight - lines * h - (lines - 1) * gap - 2 * (gap / 2);
    if (0 > top) top = 0;
    top += gap / 2;
    #else
    size_t top = 5;
    #endif

    int32_t yPos = top;
    int32_t sz = 0;

    tft.setTextColor(TFT_BROWN, TFT_BLACK);

    if (screen.refresh) {
        tft.setTextDatum(TL_DATUM);
        tft.setFreeFont(FSS9);
        tft.drawString("Channel", xPos, yPos, font);
        yPos += h + gap;
        tft.drawString("Dropped", xPos, yPos, font);
        // tft.drawString("Line", xPos, yPos, font);
        yPos += h + gap;
        // tft.drawString("TOTAL", xPos, yPos, font);
        tft.drawString("kbps", xPos, yPos, font);
    }

    tft.setTextDatum(TR_DATUM);
    tft.setFreeFont(FSS9);
    const int32_t xStart = kMaxWidth/2;
    xPos = kMaxX;
    yPos = top;
    if (screen.refresh) {
        tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
        tft.drawString(String(chView), xPos, yPos, font);
    }
    yPos += h + gap;
    tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
    tft.drawString(String(cs[i].dropped), xPos, yPos, font);
    // tft.drawString(String(not_the_one), xPos, yPos, font);
    yPos += h + gap;
    tft.fillRect(xStart, yPos, (kMaxX - xStart) - sz, h, TFT_BLACK);
    // tft.drawString(String(cs[i].total), xPos, yPos, font);
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%04u", bps);
        buf[sizeof(buf)-1] = '\0';
        size_t sz = strlen(buf);
        if (sz < sizeof(buf) - 2) {
            memmove(&buf[sz - 2], &buf[sz - 3], 3);
            buf[sz - 3] = '.';
        }
        tft.drawString(String(buf), xPos, yPos, font);
    }
    screen.refresh = false;
}
#endif
