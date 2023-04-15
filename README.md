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

I have only tested in a Linux environment. It should be easy enough to adapt the script for Windows.

Built with Arduino IDE 1.8.18 with ESP32 board add-on support.


## Hardware modules tested

**LilyGo T-Display-S3**
```
  Build with:
    Board: "LilyGo T-DisplayS3"            -DARDUINO_LILYGO_T_DISPLAY_S3=1
    USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
    USB CDC On Boot: "Disabled"            -DARDUINO_USB_CDC_ON_BOOT=0
    Upload Mode: "UART0 / Hardware CDC"
    Flash Size: "16MB (128Mb)"
    PSRAM: "OPI PSRAM"                     -DBOARD_HAS_PSRAM
```
**LilyGo T-Dongle-S3**
```
  Build with:
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
  * Packets excluded by the SDK filter are not included in the Packet or "kps" count. In contrast, Packets excluded by the custom filter have already been counted before the processing begins.


## Using `build_opt.h` or `mkbuildoptglobals.py` with Arduino ESP32

To use `mkbuildoptglobals.py`, you will need to create or update `platform.local.txt` or you
can copy the globals parts from `WiFiPcap.ino.globals.h`'s comment block to
`build_opt.h` minus the comments.

To install `mkbuildoptglobals.py` see [Global Build Options](https://github.com/mhightower83/WiFiPcap/wiki/Global-Build-Options)
