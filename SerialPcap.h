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
*/

#ifndef SERIALPCAP_H
#define SERIALPCAP_H

#include <Arduino.h>
#include <USB.h>
/*
  HWSerial:

  TODO: I think it makes sense to recast HWSerial to CONSOLE. CONSOLE is often
  used for debug printing.

  Has no value on the LilyGo T-Dongle-S3 the IO pins are not exposed.

  For the LilyGo T-Display-S3 a USB Bridge to UART needs to be connected to the
  correct GPIO pins. Use JST-SH connector next to USB Port.

  TODO: To make printing messages more productive add support to redirect them
  to the Display.
*/
#if ARDUINO_USB_CDC_ON_BOOT
// For "LilyGo T-DisplayS3" Serial0, GPIO43 and 44, are accessible on JST-SH 1.0mm 4-PIN connector
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
extern USBCDC USBSerial;
#endif


#ifdef __cplusplus
extern "C" {
#endif

// Very confused, only thinking about ESP32-S3 for now, using USB interface
#if ARDUINO_USB_MODE
// #if ARDUINO_USB_CDC_ON_BOOT    // "Serial" used for USB CDC
// // extern HWCDC Serial;
// #else
// // extern HWCDC USBSerial;
// #endif
#define SERIAL_INF HWCDC
#else
// ! ARDUINO_USB_MODE && ! ARDUINO_USB_CDC_ON_BOOT must be USB-OTG (TinyUSB)
// extern USBCDC USBSerial;
#define SERIAL_INF USBCDC
#endif


#ifndef STRUCT_PACKED
#define STRUCT_PACKED __attribute__((packed))
#endif


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define PCAP_MAGIC                    0xA1B2C3D4
#define PCAP_DEFAULT_VERSION_MAJOR    0x02    // Major Version
#define PCAP_DEFAULT_VERSION_MINOR    0x04    // Minor Version
#define PCAP_DEFAULT_TIME_ZONE_GMT    0x00    // Time Zone

#ifndef PCAP_MAX_CAPTURE_PACKET_SIZE          // To override, define a build constant
#define PCAP_MAX_CAPTURE_PACKET_SIZE  2312u   // Largest expected WiFi packet
#endif

/*
  https://www.tcpdump.org/linktypes.html
  Link layer Type Definition, used by Pcap reader, Wireshark, to decode payload
*/
typedef enum {
    PCAP_LINK_TYPE_LOOPBACK = 0,       // Loopback devices, except for later OpenBSD
    PCAP_LINK_TYPE_ETHERNET = 1,       // Ethernet, and Linux loopback devices
    PCAP_LINK_TYPE_TOKEN_RING = 6,     // 802.5 Token Ring
    PCAP_LINK_TYPE_ARCNET = 7,         // ARCnet
    PCAP_LINK_TYPE_SLIP = 8,           // SLIP
    PCAP_LINK_TYPE_PPP = 9,            // PPP
    PCAP_LINK_TYPE_FDDI = 10,          // FDDI
    PCAP_LINK_TYPE_ATM = 100,          // LLC/SNAP encapsulated ATM
    PCAP_LINK_TYPE_RAW_IP = 101,       // Raw IP, without link
    PCAP_LINK_TYPE_BSD_SLIP = 102,     // BSD/OS SLIP
    PCAP_LINK_TYPE_BSD_PPP = 103,      // BSD/OS PPP
    PCAP_LINK_TYPE_CISCO_HDLC = 104,   // Cisco HDLC
    PCAP_LINK_TYPE_802_11 = 105,       // 802.11
    PCAP_LINK_TYPE_BSD_LOOPBACK = 108, // OpenBSD loopback devices(with AF_value in network byte order)
    PCAP_LINK_TYPE_LOCAL_TALK = 114    // LocalTalk
} pcap_link_type_t;


// https://www.wireshark.org/docs/wsar_html/libpcap_8h_source.html
// https://github.com/the-tcpdump-group/libpcap/blob/master/pcap/pcap.h
/*
   Pcap File Header
 */
struct PcapFileHeader {
    uint32_t magic;     // Magic Number
    uint16_t major;     // Major Version
    uint16_t minor;     // Minor Version
    int32_t  zone;      // GMT to local correction
    uint32_t sigfigs;   // Timestamp Accuracy
    uint32_t snaplen;   // Max length of captured packets, in octets
    uint32_t link_type; // Link Layer Type
} STRUCT_PACKED;

/*
   Pcap Packet Header
 */
struct PcapPacketHeader {
    uint32_t seconds;        // Seconds since January 1st, 1970, 00:00:00 GMT
    uint32_t microseconds;   // Microseconds when the packet was captured (offset from seconds)
    uint32_t capture_length; // Bytes of captured data, less that or equal to packet_length
    uint32_t packet_length;  // Actual length of current packet
} STRUCT_PACKED;

struct WiFiPcap {  // Object to place on queue
    PcapPacketHeader pcap_header;
    uint8_t payload[];
} STRUCT_PACKED;


esp_err_t serial_pcap_start(SERIAL_INF* pcapSerial);

esp_err_t serial_pcap_cb(void *recv_buf, wifi_promiscuous_pkt_type_t type);

void serial_pcap_notifyDtrRts(bool dtr, bool rts);

void reset_dropped_count(void);

#ifdef __cplusplus
}
#endif


#endif
