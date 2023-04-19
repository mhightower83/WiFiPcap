# WiFiPcap
Stream WiFi packets encapsulated with PCAP to Wireshark. Uses the WiFi Promiscuous feature of the ESP32-S3 to capture and stream PCAP encapsulated packets to Wireshark through the USB CDC interface.

The Arduino Sketch needs to be build and flashed onto an ESP32-S3 module. The
module needs to be plugged into a USB port on your computer. Use the python
script to launch Wireshark and redirect USB CDC data into stdin of Wireshark.
The python script allows for changing of channel and setting SDK filter values
as well as some custom screening filters on the ESP32 side. The pre-filters can
help avoid dropped packets by reducing traffic through the USB interface. If the
traffic volume is not too high, you can use the `-f "all"` option to pass all
traffic and only use Wireshark's filter options.

I have mainly tested with Linux.
Should now work on Windows. If you used a custom location to install Wireshark, you will need to fix the path to Wireshark at the top of the script. Look for `wireshark_path_win32`.

Built with Arduino IDE 1.8.19 with ESP32 board add-on support.


## Hardware modules tested

**LilyGo T-Display-S3**
```
  Build with Tools selection:              (defines set)
    Board: "LilyGo T-DisplayS3"            -DARDUINO_LILYGO_T_DISPLAY_S3=1
    USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
    USB CDC On Boot: "Disabled"            -DARDUINO_USB_CDC_ON_BOOT=0
    Upload Mode: "UART0 / Hardware CDC"
    Flash Size: "16MB (128Mb)"
    PSRAM: "OPI PSRAM"                     -DBOARD_HAS_PSRAM
```
**LilyGo T-Dongle-S3**
```
  Build with Tools selection:              (defines set)
    Board: "ESP32S3-Dev Module"            -DARDUINO_ESP32S3_DEV=1
    USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
    USB CDC On Boot: "Disabled"            -DARDUINO_USB_CDC_ON_BOOT=0
    Upload Mode: "UART0 / Hardware CDC"
    Flash Size: "16MB (128Mb)"
    PSRAM: "disabled"                      none
```

**Other Modules**

It should build and work with other __ESP32-S3__ based modules that provide
access to the builtin USB interface.

**Additional libraries needed to support above modules:**
* TFT_eSPI  - https://github.com/Bodmer/TFT_eSPI
* OneButton - https://github.com/mathertel/OneButton

## Usage
Run the python script in `extras/` with `--help` for more information on how to use.

## Known Issues, Behaviors, and Nuances

* The T-Dongle-S3 fails when using the TFCard and streaming PCAP to Wireshark. Additionally, I am starting to recognize some thermal issues with my T-Dongle-S3. If you leave out the TFCard, it works fine. Also, it works fine if you use the TFCard and don't stream PCAP packets.

* Linux appears to drop key-strokes when the USB CDC is busy with Wireshark. Setting filters on the ESP32 side helps to a degree.

* When a new connection is made (as indicated by DTR going high), the dropped packet count resets. It is normal to see some dropped packets counted during disconnect. While disconnected, the queuing of packets continue; however, without a host connection, they are cleared in the worker thread and are not counted as dropped.

* Filtering:
  * SDK Filters, while in Promiscuous mode, an SDK defined filter can be registered with the SDK.
  * Custom filters (OUI, MAC, session, etc.), at callback, additional filtering can be applied.
  * Packets excluded by the SDK filter are not included in the Packet or "kbps" count. In contrast, Packets excluded by the custom filter have already been counted before the processing begins.


## Using Global macros
The issue addressed here is the absence of support in the Arduino IDE for global defines for libraries. The TFT_eSPI library needed for supporting LilyGo
T-Dongle-S3 or LilyGo T-Display-S3 requires editing a library `.h` file.

If you are not using one of these modules or that library you can skip this
issue. Other, build changes can be applied by editing `WiFiPcap.ino.globals.h`.
Or, you can follow the library maintainers recommendation and directly edit the
library files and track edits for each project that uses it. Every workaround has its own set of limitations. Personally I prefer [`mkbuildoptglobals.py`](https://github.com/mhightower83/WiFiPcap/wiki/Global-Build-Options).


There are three choices for helping TFT_eSPI find the Hardware correct `tft_setup.h` file.
1. `platform.local.txt`
2. `mkbuildoptglobals.py`
3. `build_opt.h`

### `platform.local.txt`
This may be the simplest solution. Add one of these lines to you
`platform.local.txt` file. Remember to remove it when building other projects.

For LilyGo T-Dongle-S3 with tools selection Board: "ESP32S3-Dev Module" and PSRAM: "disabled"
```
compiler.cpp.extra_flags=-I{build.source.path}/src/T-Dongle-S3/ -DARDUINO_LILYGO_T_DONGLE_S3=1
```

For LilyGo T-Display-S3 with tools selection Board: "ESP32S3-Dev Module" and PSRAM: "OPI PSRAM"
```
compiler.cpp.extra_flags=-I{build.source.path}/src/T-Display-S3/ -DARDUINO_LILYGO_T_DISPLAY_S3=1
```

### `mkbuildoptglobals.py`

To use `mkbuildoptglobals.py`, you will need to create or update `platform.local.txt`.
For instruction on installing `mkbuildoptglobals.py` see [Global Build Options](https://github.com/mhightower83/WiFiPcap/wiki/Global-Build-Options)

### `build_opt.h`

Or you can add one of these to `build_opt.h`:

For LilyGo T-Dongle-S3 with tools selection Board: "ESP32S3-Dev Module" and PSRAM: "disabled"
```
-I"/home/userid/Arduino/WiFiPcap//src/T-Dongle-S3/"
-DARDUINO_LILYGO_T_DONGLE_S3=1
```

For LilyGo T-Display-S3 with tools selection Board: "ESP32S3-Dev Module" and PSRAM: "OPI PSRAM"
```
-I"/home/userid/Arduino/WiFiPcap/src/T-Display-S3/"
-DARDUINO_LILYGO_T_DISPLAY_S3=1
```



When using `build_opt.h`, build dependencies may not always work correctly due to Arduino IDE's aggressive caching. `mkbuildoptglobals.py` handles this issue.

Otherwise, a complete shutdown of the Arduino IDE, open only one Sketch, build, and flash will work fine. With `build_opt.h`, the problem arises when you open a second Sketch and start building. Aggressive caching can use cached components that should have been rebuilt with the current Sketch's globals.
