# WiFiPcap
Stream WiFi packets encapsulated with PCAP to Wireshark

Uses the WiFi Promiscuous feature of the ESP32-S3 to capture and stream PCAP
encapsulated packets to Wireshark through the USB CDC interface.

The Arduino Sketch needs to be build and flashed onto an ESP32-S3 module. The
module needs to be plugged into a USB port on your computer. Use the python
script to launch Wireshark and redirect USB CDC data into stdin of Wireshark.
The python script allows for changing of channel and setting SDK filter values
as well as some custom screening filters on the ESP32 side. The pre-filters can
help avoid dropped packets by reducing traffic through the USB interface. If the
traffic volume is not too high, you can use the `-f "all"` option to pass all
traffic and only use Wiresharks filter options.

I have only tested in a Linux environment.

Built with Arduino IDE 1.8.18 with ESP32 board add-on support.


## Hardware modules tested

**LilyGo T-Display-S3**
```
  Build with:
    Board: "LilyGo T-DisplayS3"            -DARDUINO_LILYGO_T_DISPLAY_S3=1
    USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
    USB CDC On Boot: "Enabled"             -DARDUINO_USB_CDC_ON_BOOT=1
    Upload Mode: "UART0 / Hardware CDC"
```
**LilyGo T-Dongle-S3**
```
  Build with:
    Board: "ESP32S3-Dev Module"            -DARDUINO_ESP32S3_DEV=1
    USB Mode: "USB-OTG (TinyUSB)"          -DARDUINO_USB_MODE=0
    USB CDC On Boot: "Enabled"             -DARDUINO_USB_CDC_ON_BOOT=1
    Upload Mode: "UART0 / Hardware CDC"
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

* TFCard not supported at this time, -DUDB_MSC=1. It is not reliable. USB drive randomly disconnects.

* Linux appears to drop key-strokes when the USB CDC is busy with Wireshark. Setting filters on the ESP32 side helps to a degree.

* The dropped packet count is set to 0 when a new connection is made as indicated by DTR going high. It is normal to see some dropped packets counted during disconnect. While disconnected, the queuing of packet continues; however, without a host connection, they are cleared in the worker thread and are not counted as dropped.

* Filtering:
  * While in Promiscuous mode, a filters can be registered with the SDK
  * Additionally at callback additional filtering can be applied, custom filters.
  * Packets excluded by the SDK filter are not included in the Packet or "kps" count. In contrast, Packets excluded by the custom filter have already been counted before the processing begins.


## Using `build_opt.h` or `mkbuildoptglobals.py` with Arduino ESP32

To use `mkbuildoptglobals.py`, you need to update `platform.local.txt` or you
can copy the globals parts from `WiFiPcap.ino.globals.h`'s comment block to
`build_opt.h` minus the comments. Details about the global comment block can be
read [here](https://github.com/esp8266/Arduino/blob/master/doc/faq/a06-global-build-options.rst)

To use `mkbuildoptglobals.py`, get a copy of [`mkbuildoptglobals.py`](https://raw.githubusercontent.com/esp8266/Arduino/master/tools/mkbuildoptglobals.py) from the Arduino ESP8266 project. It will be in the `./tools/` folder. Place it in the `./tools/` folder of Arduino ESP32.

Add these updates to your `platform.local.txt` file:
```
# This block add support for SketchName.ino.globals.h to an ESP32 build
#
# To avoid build confusion when using SketchName.ino.globals.h, avoid using build_opt.h
#
# These would disable build_opt.h
# recipe.hooks.prebuild.5.pattern.windows=
# recipe.hooks.prebuild.5.pattern=
#
# Fully qualified file names for processing sketch global options
globals.h.source.fqfn={build.source.path}/{build.project_name}.globals.h
commonhfile.fqfn={build.core.path}/CommonHFile.h
build.opt.fqfn={build.path}/core/build.opt
build.opt.flags="@{build.opt.fqfn}"
mkbuildoptglobals.extra_flags=
runtime.tools.mkbuildoptglobals={runtime.platform.path}/tools/mkbuildoptglobals.py
#
# "recipe.hooks.prebuild.9.pattern=" should be a new entry
recipe.hooks.prebuild.9.pattern=python3 -I "{runtime.tools.mkbuildoptglobals}" "{runtime.ide.path}" {runtime.ide.version} "{build.path}" "{build.opt.fqfn}" "{globals.h.source.fqfn}" "{commonhfile.fqfn}" {mkbuildoptglobals.extra_flags}
#
# These lines override existing entries in platform.txt. When platform.txt is changed these may need editing.
compiler.c.flags={compiler.c.flags.{build.mcu}} {compiler.warning_flags} {compiler.optimization_flags} {build.opt.flags}
compiler.cpp.flags={compiler.cpp.flags.{build.mcu}} {compiler.warning_flags} {compiler.optimization_flags} {build.opt.flags}
compiler.S.flags={compiler.S.flags.{build.mcu}} {compiler.warning_flags} {compiler.optimization_flags} {build.opt.flags}
```
