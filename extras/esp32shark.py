#!/usr/bin/env python

'''
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
'''

'''
  This script does not create a pcap file. That task is left to Wireshark.
  This script connects the ESP32 using USB CDC to stdin of Wireshark, after
  setting ESP32's filter, channel, and time options.

  References:
    Wireshark https://wiki.wireshark.org/CaptureSetup/Pipes.md
'''

import traceback
import sys
import argparse
import textwrap
import locale

import serial
import io
import os
import subprocess
import shlex
import signal
import time
import re
# https://stackoverflow.com/a/52809180
import serial.tools.list_ports

serialport = ""
bpsRate = 9216000
# bpsRate = 115200
esp32_name = "WiFiPcap"
docs_url = f"https://github.com/mhightower83/{esp32_name}/wiki"

# Regional values - The WiFiPcap is restricted by CONFIG_WIFIPCAP_CHANNEL_MAX in KConfig.h
# max_channel = 14
# max_channel = 13    # Europe
# https://en.wikipedia.org/wiki/List_of_WLAN_channels#endnote_B
# max_channel = 13    # North America - permitted with channels 12 & 13 at low power
max_channel = 11    # North America - more common range

# not using - keeping this for now
# retrieve *system* encoding, not the one used by python internally
if sys.version_info >= (3, 11):
    def get_encoding():
        return locale.getencoding()
else:
    def get_encoding():
        return locale.getdefaultlocale()[1]


def parseArgs():
    global docs_url
    name = os.path.basename(__file__)
    extra_txt = f'''\
       Defaults to starting capture with {esp32_name} using current WiFi Channel
       and set to GMT based on Host System's time. The captured stream is piped
       into Wireshark which begins capturing to a file.

       Use '--filter_...' options to reduce traffic through the USB CDC interface.
       Think of this as a stage 1 filter in front of Wireshark.
       When the filter option is omitted, the previous filter uploaded is used.

       Examples:

         {name} --filter_session --ch=11

         {name} -c6 --filter_mask "mgmt|data"

         {name} -c1 --filter_all

         {name} -c6 --filter "mgmt|data" --oui "00:DD:00" --multicast

       These mnemonics represent filter options offered by the ESP32 SDK.
       Join these mnemonics with '|' to construct a FILTER_MASK:

         all_mask    Keep all packets
         fcsfail     FCS failed packets, also includes bad packets

           Packets with type:
         mgmt          WIFI_PKT_MGMT
         ctrl          WIFI_PKT_CTRL
         data          WIFI_PKT_DATA
         misc          WIFI_PKT_MISC

         mpdu        MPDU a kind of WIFI_PKT_DATA
         ampdu       AMPDU a kind of WIFI_PKT_DATA

           WIFI_PKT_CTRL subtypes:
         wrapper       Control Wrapper
         bar           Block Ack Request
         ba            Block Ack
         pspoll        PS-Poll
         rts           RTS
         cts           CTS
         ack           ACK
         cfend         CF-END
         cfendack      CF-END+CF-ACK
         ctrl_mask     All WIFI_PKT_CTRL subtypes

           Custom - not in the ESP32 SDK
         session    Uses logic in callback function to select packets related
                    to AP connections
         fcslen     Experimental, FCS Length include in packet length

         0x10000    Hex constant are also supported

       more help at {docs_url}
       '''
    parser = argparse.ArgumentParser(
        description=f'Pipes {esp32_name} into Wireshark',
        formatter_class=argparse.RawDescriptionHelpFormatter,
              epilog=textwrap.dedent(extra_txt))
    parser.add_argument('--channel','-c', '--ch', type=int, choices=range(1, max_channel+1), required=False, default=None, help='Select/Change WiFi Channel')
    parser.add_argument('--no_time_sync', '-n', dest='time_sync', action='store_false', required=False, default=True, help=f'No time sync between Host and {esp32_name}.')
    parser.add_argument('--port', '-p', required=False, default=None, help=f'Full device path for USB CDC device connected to {esp32_name}.')
    parser.add_argument('--zc', dest='channel', type=int, choices=range(1, 15), required=False, default=None, help=argparse.SUPPRESS)   # debug
    parser.add_argument('--testing', '--test', '-t', action='store_true', default=None, help=argparse.SUPPRESS)     # debug - skips starting Wireshark


    group2 = parser.add_mutually_exclusive_group(required=False)
    group2.add_argument('--unicast', '-u', '--mac', required=False, default=None, help=f'unicast/MAC, 6 bytes of Source or Destination Address of interest expressed in hex and quoted, with "-" or ":" for byte separator')
    group2.add_argument('--oui', '-o', required=False, default=None, help=f'OUI, first 3 bytes of Source or Destination Address of interest in quotes with "-" or ":" separator')
    group2.add_argument('--no_addr', action='store_true', required=False, default=None, help=f'Clear Unicast/OUI filter option')

    group3 = parser.add_mutually_exclusive_group(required=False)
    group3.add_argument('--multicast', '-m', nargs='?', action='store', required=False, default=None, const="01:00:00:00:00:00", help=f'Use with --unicast or --oui to capture multicast responses. First 3 or 6 bytes of a Multicast Address in hex and quoted, with "-" or ":" for byte separator. For a 3 byte value pad with zeros to 6 bytes.')
    group3.add_argument('--broadcast', '-b', action='store_true', required=False, default=None, help=f'Use with --unicast or --oui to capture broadcast responses.')


    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument('--filter_mask', '-f', '--filter', required=False, default=None, help='Specify WiFi filter mask. See example and mnemonic list below.')
    group.add_argument('--filter_all', '-a', action='store_true', default=None, help='Capture all packets possible, includes type control and bad packets')
    group.add_argument('--filter_good', '-g', action='store_true', default=None, help='Capture all good packets possible, includes type control')
    # This one should be the default
    group.add_argument('--filter_session', action='store_true', default=None, help='Capture AP connection and Data related packets')
    return parser.parse_args()
    # ref epilog, https://stackoverflow.com/a/50021771
    # ref nargs='*'', https://stackoverflow.com/a/4480202
    # ref no '--n' parameter, https://stackoverflow.com/a/21998252
    #
    # examples for reference
    # group.add_argument('--cache_core', action='store_true', default=None, help='Assume a "compiler.cache_core" value of true')
    # group.add_argument('--no_cache_core', dest='cache_core', action='store_false', help='Assume a "compiler.cache_core" value of false')
    # group.add_argument('--preferences_env', nargs='?', action='store', type=check_env, const="ARDUINO15_PREFERENCES_FILE", help=argparse.SUPPRESS)


def processFilter(filter_str, filter_good, filter_all, filter_session):
    """
    These values are based on "./esp32s3/include/esp_wifi/include/esp_wifi_types.h"
    At this writing, ESP32, ESP32-S2, ESP32-S2, and ESP32-C3 have identical "esp_wifi_types.h" files.
    """
    # The SDK uses a value of 0xFFFFFFFF for WIFI_PROMIS_FILTER_MASK_ALL. We borrow some unused bit possitions for some custom options
    # And, replace k_filter_all with k_filter_mask_all later.
    k_filter_all            = (0xFF80007F)  # Mask of know filter bits used by SDK, needs to be verified with each new SDK update
    k_filter_mask_all       = (0xFFFFFFFF)  # WIFI_PROMIS_FILTER_MASK_ALL,           filter/keep all packets
    k_filter_mgmt           = (1<<0)        # WIFI_PROMIS_FILTER_MASK_MGMT,          packets w/type WIFI_PKT_MGMT
    k_filter_ctrl           = (1<<1)        # WIFI_PROMIS_FILTER_MASK_CTRL,          packets w/type WIFI_PKT_CTRL
    k_filter_data           = (1<<2)        # WIFI_PROMIS_FILTER_MASK_DATA,          packets w/type WIFI_PKT_DATA
    k_filter_misc           = (1<<3)        # WIFI_PROMIS_FILTER_MASK_MISC,          packets w/type WIFI_PKT_MISC
    k_filter_data_mpdu      = (1<<4)        # WIFI_PROMIS_FILTER_MASK_DATA_MPDU,     MPDU a kind of WIFI_PKT_DATA
    k_filter_data_ampdu     = (1<<5)        # WIFI_PROMIS_FILTER_MASK_DATA_AMPDU,    AMPDU a kind of WIFI_PKT_DATA
    k_filter_fcsfail        = (1<<6)        # WIFI_PROMIS_FILTER_MASK_FCSFAIL,       FCS failed packets
    k_filter_ctrl_mask_all  = (0xFF800000)  # WIFI_PROMIS_CTRL_FILTER_MASK_ALL,      filter/keep all control packets
    k_filter_ctrl_wrapper   = (1<<23)       # WIFI_PROMIS_CTRL_FILTER_MASK_WRAPPER,  WIFI_PKT_CTRL w/subtype Control Wrapper
    k_filter_ctrl_bar       = (1<<24)       # WIFI_PROMIS_CTRL_FILTER_MASK_BAR,      WIFI_PKT_CTRL w/subtype Block Ack Request
    k_filter_ctrl_ba        = (1<<25)       # WIFI_PROMIS_CTRL_FILTER_MASK_BA,       WIFI_PKT_CTRL w/subtype Block Ack
    k_filter_ctrl_pspoll    = (1<<26)       # WIFI_PROMIS_CTRL_FILTER_MASK_PSPOLL,   WIFI_PKT_CTRL w/subtype PS-Poll
    k_filter_ctrl_rts       = (1<<27)       # WIFI_PROMIS_CTRL_FILTER_MASK_RTS,      WIFI_PKT_CTRL w/subtype RTS
    k_filter_ctrl_cts       = (1<<28)       # WIFI_PROMIS_CTRL_FILTER_MASK_CTS,      WIFI_PKT_CTRL w/subtype CTS
    k_filter_ctrl_ack       = (1<<29)       # WIFI_PROMIS_CTRL_FILTER_MASK_ACK,      WIFI_PKT_CTRL w/subtype ACK
    k_filter_ctrl_cfend     = (1<<30)       # WIFI_PROMIS_CTRL_FILTER_MASK_CFEND,    WIFI_PKT_CTRL w/subtype CF-END
    k_filter_ctrl_cfendack  = (1<<31)       # WIFI_PROMIS_CTRL_FILTER_MASK_CFENDACK, WIFI_PKT_CTRL w/subtype CF-END+CF-ACK

    k_filter_custom_session = (1<<16)       # Internal to WiFiPcap, not an SDK value.
                                            # Capture packets related to an AP connection
                                            # Removes null subtypes and noisy beacons and probes

    k_filter_custom_fcslen  = (1<<17)       # Internal to WiFiPcap, not an SDK value.
                                            # Experimental length includes fcs

    k_filter_custom_badpkt  = (1<<18)       # keep bad packets

    k_filter_custom_mask    = (0x00070000)

    k_filter_table = {
        "all":       k_filter_all,              # filter/keep all packets
        "all_mask":  k_filter_all,              # filter/keep all packets
        "good":      (k_filter_all & ~k_filter_fcsfail),
        #                                         packets with type:
        "mgmt":      k_filter_mgmt,             #   WIFI_PKT_MGMT
        "ctrl":      k_filter_ctrl,             #   WIFI_PKT_CTRL
        "data":      k_filter_data,             #   WIFI_PKT_DATA
        "misc":      k_filter_misc,             #   WIFI_PKT_MISC
        "mpdu":      k_filter_data_mpdu,        # MPDU a kind of WIFI_PKT_DATA
        "ampdu":     k_filter_data_ampdu,       # AMPDU a kind of WIFI_PKT_DATA
        "fcsfail":   k_filter_fcsfail,          # FCS failed packets
        #
        "ctrl_mask": k_filter_ctrl_mask_all,    # All WIFI_PKT_CTRL subtypes
        #                                         WIFI_PKT_CTRL with subtypes:
        "wrapper":   k_filter_ctrl_wrapper,     #   Control Wrapper
        "bar":       k_filter_ctrl_bar,         #   Block Ack Request
        "ba":        k_filter_ctrl_ba,          #   Block Ack
        "pspoll":    k_filter_ctrl_pspoll,      #   PS-Poll
        "rts":       k_filter_ctrl_rts,         #   RTS
        "cts":       k_filter_ctrl_cts,         #   CTS
        "ack":       k_filter_ctrl_ack,         #   ACK
        "cfend":     k_filter_ctrl_cfend,       #   CF-END
        "cfendack":  k_filter_ctrl_cfendack,    #   CF-END+CF-ACK
        #
        "session":   (k_filter_custom_session | k_filter_mgmt | k_filter_data),   # Capture packets related to an AP connection
        "fcslen":    k_filter_custom_fcslen,    # Experimental - FCS length include in packet length
        "bad":       (k_filter_custom_badpkt | k_filter_fcsfail),    # Bad packets
        "custom_mask": k_filter_custom_mask }

    supported_mnemonics = "all|all_mask|good|mgmt|ctrl|data|misc|mpdu|ampdu|fcsfail|ctrl_mas|wrapper|bar|ba|pspoll|rts|cts|ack|cfend|cfendack|session|fcslen"

    use_filter = None
    use_custom_filter = None
    custom_filter = 0
    filter_mask = 0
    if filter_str:
        items = filter_str.split('|')
        for key in items:
            if key.startswith("0x"):
                filter_mask |= int(key, 0)
            else:
                try:
                    filter_mask |= k_filter_table[key]
                except:
                    print(f'[!] Unknown filter mnemonic: "{key}"')
                    print(f'[!] Supported mnemonics: "{supported_mnemonics}"')
                    raise Exception(f'Unknown filter mnemonic: "{key}"')

        if k_filter_ctrl_mask_all & filter_mask:
            filter_mask = k_filter_ctrl | filter_mask


    # compose required bits to support selection
    if filter_mask:
        use_custom_filter = (filter_mask & k_filter_custom_mask)
        use_filter = filter_mask & ~k_filter_custom_mask
    elif filter_good:
        use_filter = k_filter_mask_all & ~k_filter_fcsfail
    elif filter_all:
        use_filter = k_filter_mask_all
    elif filter_session:
        use_filter = k_filter_data | k_filter_mgmt
        use_custom_filter = k_filter_custom_session

    if (use_filter & k_filter_all) == k_filter_all:
        # If all the known SDK bits are set then most likely this should be
        # all ones like the SDK value fro WIFI_PROMIS_FILTER_MASK_ALL.
        use_filter = k_filter_mask_all

    return [ use_filter, use_custom_filter ]


def pickPort():
    ports = serial.tools.list_ports.comports()
    sortedports = sorted(ports)
    portcount = 0
    for port, desc, hwid in sortedports:
        # print("{}: {} [{}]".format(port, desc, hwid))
        portcount += 1
        print("[+] {} - {} - {}".format(portcount, port, desc))

    maxport = portcount
    if portcount == 0:
        print("[+] No Serial ports found")
        return None
    elif portcount == 1:
        serialport = sortedports[0].device
    else:
        try:
            choice = 1;
            chose = input(f'[?] Select a serial port (default "{choice}"): ')
            if chose != "":
                choice = int(chose)
            if choice > portcount:
                print("\n[+] Not a valid selection")
                return None
            serialport = sortedports[choice-1].device
        except KeyboardInterrupt:
            return None

    print(f'[*] Using serial port "{serialport}"')
    return serialport


def connectESP32(port, channel, filter, unicast, multicast, time_sync):
    global bpsRate

    canBreak = False
    while not canBreak:
        try:
            fd = serial.Serial(port, bpsRate)
            canBreak = True
        except KeyboardInterrupt:
            return None
        except:
            print(f'[!] Serial port "{port}" open attempt failed!')
            return None

    print(f'[+] Connected to serial port: "{fd.name}"')

    while True:
        try:
            line = fd.readline()
        except KeyboardInterrupt:
            return None
        except:
            print("[!] Serial port connection closed/failed while reading port!")
            return None

        print(f'[>] ESP32 -> "{line.decode()[:-1]}"')
        if b"<<SerialPcap>>" in line:
            print("[+] Uploading options ...")
            break

    str = "P"
    if channel:
        str += f'C{channel}'

    if filter[0] != None:                   # SDK filter
        val = 0x0FFFF & (filter[0] >> 16)
        str += f'F{val}'
        val = 0x0FFFF & filter[0]
        str += f'f{val}'
    if filter[1] != None:                   # custome filter
        val = 0x0FFFF & (filter[1] >> 16)   #   only uses the upper 16 bits.
        str += f'S{val}'
    else:
        str += f'S0'

    if unicast:
        str += f'U{unicast[0]}u{unicast[1]}'
        if multicast:
            str += f'M{multicast[0]}m{multicast[1]}'
        else:
            str += f'M0m0'

    if time_sync:
        now = time.time_ns()    # returns time as an integer number of nanoseconds since the epoch
        microseconds = round(now / 1000)
        seconds = int(microseconds / 1000000)
        microseconds %= 1000000
        str += f'G{seconds}g{microseconds}X\n'
    else:
        str += f'X\n'

    cmd = str.encode()
    fd.write(cmd)
    fd.flush()
    print("[<] ESP32 <- {}".format(cmd))

    while True:
        try:
            line = fd.readline()
        except KeyboardInterrupt:
            return None
        except:
            print("[!] Serial port connection closed/failed while reading port!")
            return None

        print(f'[>] ESP32 -> "{line.decode()[:-1]}"')
        if b"<<PASSTHROUGH>>" in line:
            print("[+] Upload Complete ...")
            break

    print("[+] Stream started ...")
    return fd


def runWireshark(fd):
    print("[+] Starting Wireshark ...")
    cmd='wireshark -k -i -'
    proc=subprocess.Popen(shlex.split(cmd), stdin=fd, preexec_fn=os.setsid)
    proc.communicate()


def processAddress(unicast, oui):
    # unicast parsing also works for multicast
    if unicast:
        addr = re.split(':|,|-|\.| ', unicast)
        if 6 != len(addr):
            print(f'[!] Bad formatting "{unicast}" should be 6 bytes long')
            raise Exception(f'Bad address formatting')
            return [0, 0]
        msb = int(addr[0], 16)*(256*256) + int(addr[1], 16)*256 + int(addr[2], 16)
        lsb = int(addr[3], 16)*(256*256) + int(addr[4], 16)*256 + int(addr[5], 16)
    elif oui:
        addr = re.split(':|,|-|\.| ', oui)
        if 3 != len(addr):
            print(f'[!] Bad formatting "{oui}" should be 3 bytes long')
            raise Exception(f'Bad address formatting')
            return [0, 0]
        msb = int(addr[0], 16)*(256*256) + int(addr[1], 16)*256 + int(addr[2], 16)
        lsb = 0
    else:
        return None

    return [ msb, lsb ]


def main():
    default_encoding = get_encoding()

    try:
        args = parseArgs()
        if args.no_addr:
            unicast = [0, 0]
            multicast = None
        else:
            unicast = processAddress(args.unicast, args.oui)
            if args.broadcast:
                multicast = [ 0x0FFFFFF, 0x0FFFFFF ]
            else:
                multicast = processAddress(args.multicast, None)
        filter = processFilter(args.filter_mask, args.filter_good, args.filter_all, args.filter_session)
    except:
        print("[+] Exiting ...")
        return 1

    if args.port:
        port = args.port
    else:
        port = pickPort()
        if not port:
            print("[+] Exiting ...")
            return 1

    print(f'[+] port          ="{port}"')
    print(f'[+] channel       ="{args.channel}"')
    if filter[0]:
        print(f'[+] filter_mask   ="{filter[0]:#08x}"')
    else:
        print('[+] filter_mask    ="None"')

    if filter[1]:
        print(f'[+] custom_filter ="{filter[1]:#08x}"')

    if unicast:
        print(f'[+] unicast       ="{unicast}"')
    # elif oui:
    #     print(f'[+] --oui="{oui}"')

    if multicast:
        print(f'[+] multicast     ="{multicast}"')

    print(f'[+] set time      ="{args.time_sync}"')
    # sys.stdout.flush()

    fd = connectESP32(port, args.channel, filter, unicast, multicast, args.time_sync)
    if None == fd:
        print("[+] Exiting ...")
        return 1

    if not args.testing:
        runWireshark(fd)

    fd.close()
    print("[+] Done.")
    return 0


if __name__ == '__main__':
    rc = 1
    try:
        rc = main()
    except:
        print(traceback.format_exc())
        print("[!] Oops!")
    sys.exit(rc)
