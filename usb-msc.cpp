/*

  A similar Sketch ./esp32/libraries/USB/examples/USBMSC/USBMSC.ino

  Reference
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/sdmmc_host.html#supported-speed-modes
  Hmm, https://www.sdcard.org/developers/sd-standard-overview/low-voltage-signaling/#:~:text=UHS%2DI%20adopted%201.8V,support%203.3V%20signaling%2C%20too.

  Speed modes not supported at present:
    * High Speed DDR mode, 8-line eMMC
    * UHS-I 1.8 V modes, 4-line SD cards

  What I think this means is we are operating the device in 3.3 V compatibility
  mode.

  If not using both USBCDC and USBMSC at the same time, it can run for 1 hour
  before USBCDC stalls and the interface disappears on the host side. MSC seems
  to still be working.

  For a more stable tool build without USBMSC support, ""-DUSE_USB_MSC=0"

  New observation, part of my stability issues may be overheating.
  My T-Dongle-S3 was not receiving any packets when I plugged it in.
  The same code built for the T-Display-S3 worked. After an hour or two to
  cool down I tried the T-Dongle-S3 and it worked again. Display and TFCard
  increase heat. Maybe I should power off the display or dim.

  I don't think I can judge if this works or not with my T-Dongle-S3.
*/
#if USE_USB_MSC


#if ARDUINO_USB_MODE
#pragma message("This sketch should be used when USB is in OTG mode")
#endif

#include <Arduino.h>
#include <USB.h>
#include <USBMSC.h>

#if ARDUINO_USB_MODE

#if ARDUINO_USB_CDC_ON_BOOT  //Serial used for USB CDC
#define HWSerial Serial0
// HWCDC Serial;
#define USBSerial Serial
#else
#define HWSerial Serial
// HWCDC USBSerial; already in HWCDC.cpp
extern HWCDC USBSerial;
#endif

#else

#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
extern USBCDC USBSerial;
#endif

#endif



#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include <esp_vfs_fat.h>

#if ARDUINO_LILYGO_T_DONGLE_S3
#include "src/T-Dongle-S3/pin_config.h"
#else
#pragma message("Add pin_config.h for your module to src/...")
#endif

#include <sdmmc_cmd.h>
#include "usb-msc.h"

USBMSC MSC;
#define MOUNT_POINT "/sdcard"
sdmmc_card_t *card;

static struct {
  esp_err_t sd=-1;
  bool msc=false;
} usb_err;

/*
  Initialize SD Driver that allows the ESP32 to read and write from the
  connected SD Card. Used to service request from the MSC Class.
*/
esp_err_t sd_init(void) {
    const char mount_point[] = MOUNT_POINT;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,                   // Maximum number of open files
        /*
           If format_if_mount_failed is set, and mount fails, format the card
           with given allocation unit size. Must be a power of 2, between sector
           size and 128 * sector size.

           For SD cards, sector size is always 512 bytes. For wear_levelling,
           sector size is determined by CONFIG_WL_SECTOR_SIZE option.
           (ED. CONFIG_WL_SECTOR_SIZE=4096)

           Using larger allocation unit size will result in higher read/write
           performance and higher overhead when storing small files.

           Setting this field to 0 will result in allocation unit set to the
           sector size.
        */
        //+ .allocation_unit_size = 16 * 1024 // Based on the above, this is a bit large
        .allocation_unit_size = 0 // Based on the above, this is a bit large
    };
    /*
      sdmmc_host_t structure initializer for SDMMC peripheral

      Uses SDMMC peripheral, with 4-bit mode enabled, and max frequency set to default 20MHz.
      A lot of this is from SDMMC_HOST_DEFAULT in sdmmc_host.h; however, .flags is different.
      Reference SD_MMC.cpp
    */
    #if 0
    sdmmc_host_t host = {
        .flags = SDMMC_HOST_FLAG_4BIT | SDMMC_HOST_FLAG_DDR, // ?? SDMMC_HOST_FLAG_DDR no in SD_MMC
        .slot = SDMMC_HOST_SLOT_1,
        .max_freq_khz = SDMMC_FREQ_DEFAULT, // BOARD_MAX_SDMMC_FREQ
        .io_voltage = 3.3f,
        .init = &sdmmc_host_init,
        .set_bus_width = &sdmmc_host_set_bus_width,
        .get_bus_width = &sdmmc_host_get_slot_width,
        .set_bus_ddr_mode = &sdmmc_host_set_bus_ddr_mode,
        .set_card_clk = &sdmmc_host_set_card_clk,
        .do_transaction = &sdmmc_host_do_transaction,
        .deinit = &sdmmc_host_deinit,
        .io_int_enable = sdmmc_host_io_int_enable,
        .io_int_wait = sdmmc_host_io_int_wait,
        .command_timeout_ms = 0,
    };
    #else
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // For using SDMMC_HOST_FLAG_DDR and SDMMC_FREQ_HIGHSPEED (40MHz)
    // SDMMC_HOST_FLAG_DDR require clean signals
    // Review https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html#ddr-mode-for-emmc-chips
    host.flags = SDMMC_HOST_FLAG_4BIT; //| SDMMC_HOST_FLAG_DDR;
    // host.max_freq_khz = BOARD_MAX_SDMMC_FREQ; // default SDMMC_FREQ_DEFAULT; // 20MHz
    #endif
#if 0
    // Configuration of SDMMC host slot - based on SDMMC_SLOT_CONFIG_DEFAULT in sdmmc_host.h
    // ESP32-S3 SDMMC Host can be configured to use arbitrary GPIOs for each of the signals.
    sdmmc_slot_config_t slot_config = {
        .clk = (gpio_num_t)SD_MMC_CLK_PIN,
        .cmd = (gpio_num_t)SD_MMC_CMD_PIN,
        .d0 = (gpio_num_t)SD_MMC_D0_PIN,
        .d1 = (gpio_num_t)SD_MMC_D1_PIN,
        .d2 = (gpio_num_t)SD_MMC_D2_PIN,
        .d3 = (gpio_num_t)SD_MMC_D3_PIN,
        .d4 = GPIO_NUM_NC,
        .d5 = GPIO_NUM_NC,
        .d6 = GPIO_NUM_NC,
        .d7 = GPIO_NUM_NC,
        .cd = SDMMC_SLOT_NO_CD,   // card present indicator not available
        .wp = SDMMC_SLOT_NO_WP,   // card write protection indicator not available
        .width = 4, // SDMMC_SLOT_WIDTH_DEFAULT, // Same as SD_MMC.cpp
        .flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP,
    };
#else
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = (gpio_num_t)SD_MMC_CLK_PIN;
    slot_config.cmd = (gpio_num_t)SD_MMC_CMD_PIN;
    slot_config.d0 = (gpio_num_t)SD_MMC_D0_PIN;
    slot_config.d1 = (gpio_num_t)SD_MMC_D1_PIN;
    slot_config.d2 = (gpio_num_t)SD_MMC_D2_PIN;
    slot_config.d3 = (gpio_num_t)SD_MMC_D3_PIN;
    slot_config.d4 = GPIO_NUM_NC;
    slot_config.d5 = GPIO_NUM_NC;
    slot_config.d6 = GPIO_NUM_NC;
    slot_config.d7 = GPIO_NUM_NC;
    slot_config.cd = SDMMC_SLOT_NO_CD;
    slot_config.wp = SDMMC_SLOT_NO_WP;
    slot_config.width = 4;
    slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // This should ensure all the pins are in the same state
#endif
#if 0
    // The connector also has 10K Pullups.
    // ?? Note SD_MMC_CLK_PIN was not included here ??
    // Is this redundant to above `.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP` line?
    gpio_set_pull_mode((gpio_num_t)SD_MMC_CMD_PIN, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode((gpio_num_t)SD_MMC_D0_PIN, GPIO_PULLUP_ONLY);  // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode((gpio_num_t)SD_MMC_D1_PIN, GPIO_PULLUP_ONLY);  // D1, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)SD_MMC_D2_PIN, GPIO_PULLUP_ONLY);  // D2, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)SD_MMC_D3_PIN, GPIO_PULLUP_ONLY);  // D3, needed in 4- and 1-line modes
#else
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html?highlight=gpio_set_drive_capability#_CPPv425gpio_set_drive_capability10gpio_num_t16gpio_drive_cap_t
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html?highlight=gpio_set_drive_capability#_CPPv416gpio_drive_cap_t
// Drive Strengths
//   * 0: ~10 mA
//   * 1: ~20 mA
//   * 2: ~40 mA  - default
//   * 3: ~80 mA
    // constexpr gpio_drive_cap_t drive_strength = GPIO_DRIVE_CAP_0; // Weak
    constexpr gpio_drive_cap_t drive_strength = GPIO_DRIVE_CAP_1; // stronger
    // constexpr gpio_drive_cap_t drive_strength = GPIO_DRIVE_CAP_2; // medium, GPIO_DRIVE_CAP_DEFAULT
    // constexpr gpio_drive_cap_t drive_strength = GPIO_DRIVE_CAP_3; // strongest, GPIO_DRIVE_CAP_MAX
    gpio_set_drive_capability((gpio_num_t)SD_MMC_CLK_PIN, GPIO_DRIVE_CAP_0);    // reduce ringing on clock
    gpio_set_drive_capability((gpio_num_t)SD_MMC_CMD_PIN, drive_strength);
    gpio_set_drive_capability((gpio_num_t)SD_MMC_D0_PIN,  drive_strength);
    gpio_set_drive_capability((gpio_num_t)SD_MMC_D1_PIN,  drive_strength);
    gpio_set_drive_capability((gpio_num_t)SD_MMC_D2_PIN,  drive_strength);
    gpio_set_drive_capability((gpio_num_t)SD_MMC_D3_PIN,  drive_strength);
#endif
    usb_err.sd = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (usb_err.sd != ESP_OK) {
        // This may not going to be visable
        if (usb_err.sd == ESP_FAIL) {
            HWSerial.printf("Failed to mount filesystem. "
                              "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.\n");
        } else
        if (usb_err.sd == ESP_ERR_INVALID_STATE) {
            HWSerial.printf("SD Card Already mounted\n");
        } else {
            HWSerial.printf("Failed to initialize the card (%s). "
                             "Make sure SD card lines have pull-up resistors in place.\n",
                             esp_err_to_name(usb_err.sd));
        }
    }
    return usb_err.sd;
}

/*
  Allow void pointer math as if byte pointer
*/
inline static void *ptrPlusOff(void *start, size_t offset) {
    return (void *)((uintptr_t)start + offset);
}
/*
  Support for MSC Class interactions
*/
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    // HWSerial.printf("MSC WRITE: lba: %u, offset: %u, bufsize: %u\n", lba, offset, bufsize);
    uint32_t count = (bufsize / card->csd.sector_size);
    sdmmc_write_sectors(card, ptrPlusOff(buffer, offset), lba, count);
    return bufsize;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    // HWSerial.printf("MSC READ: lba: %u, offset: %u, bufsize: %u\n", lba, offset, bufsize);
    uint32_t count = (bufsize / card->csd.sector_size);
    sdmmc_read_sectors(card, ptrPlusOff(buffer, offset), lba, count);
    return bufsize;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
    HWSerial.printf("MSC START/STOP: power: %u, start: %u, eject: %u\n", power_condition, start, load_eject);
    return true;
}

#if 0
/*
  General USB event handling

  HWSerial printing - tree falling in the forest.
  Maybe later use that fancy LCD Display for these messages.
*/
static void usbMscEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == ARDUINO_USB_EVENTS) {
        arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
        switch (event_id) {
            case ARDUINO_USB_STARTED_EVENT:
                HWSerial.println("USB PLUGGED");
                break;
            case ARDUINO_USB_STOPPED_EVENT:
                HWSerial.println("USB UNPLUGGED");
                break;
            case ARDUINO_USB_SUSPEND_EVENT:
                HWSerial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en);
                break;
            case ARDUINO_USB_RESUME_EVENT:
                HWSerial.println("USB RESUMED");
                break;

            default:
                break;
        }
    }
}
#endif

bool setupMsc() {
    // Device presented to the PC's USB connection
    // Not sure who sees these. lsusb does not report any of it.
    // Must be at a different level
    MSC.vendorID("LILYGO");       // max 8 chars
    MSC.productID("T-Dongle-S3"); // max 16 chars
    MSC.productRevision("1.0");   // max 4 chars
    MSC.onStartStop(onStartStop);
    MSC.onRead(onRead);
    MSC.onWrite(onWrite);
    MSC.mediaPresent(true);
    usb_err.msc = MSC.begin(card->csd.capacity, card->csd.sector_size);
    return usb_err.msc;
}

void sd_end(void) {
    const char mount_point[] = MOUNT_POINT;
    // if (usb_err.msc) MSC.end();
    // if (usb_err.sd == ESP_OK)
    esp_vfs_fat_sdcard_unmount(mount_point, card);
}

#endif // #if ARDUINO_USB_MODE
