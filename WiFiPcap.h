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

#ifndef WIFIPCAP_H
#define WIFIPCAP_H
/*
  Possible points of confusion

  Network order, or big endian, presents most-significant byte (octet) first and
  of the  1st octet the least significant bit shifts out onto the wire first.
  The most significant byte is stored in the lower address and the least
  significant byte in the highest.

  The ESP is little endian. Least significant byte stored in the lower address
  and so on.

*/


////////////////////////////////////////////////////////////////////////////////
// Some defines so we don't need utils/common.h and all of its conflicts
// to use "ieee802_11_defs.h" from wpa_supplicant
typedef int8_t s8;
typedef uint8_t u8;
typedef uint16_t le16;
typedef uint32_t le32;
typedef uint64_t le64;
#define ETH_ALEN (6)
#include "src/ieee802_11_defs.h"

constexpr uint8_t maxChannel = CONFIG_WIFIPCAP_CHANNEL_MAX; // Regional value
/*
  Internal to WiFiPcap, not an SDK value.
  Capture packets related to an AP connection
  Removes null subtypes and noisy beacons and probes
*/
constexpr uint32_t k_filter_custom_session = (1<<16);
constexpr uint32_t k_filter_custom_fcslen = (1<<17);
constexpr uint32_t k_filter_custom_badpkt = (1<<18);
constexpr uint32_t k_filter_all_known_sdk_bits = (0xFF80007Fu);

//D constexpr size_t k_pass_multicast_count = 16;

#define STRUCT_PACKED __attribute__((packed))

struct MacAddr {
    uint8_t mac[ETH_ALEN];
} STRUCT_PACKED;

const MacAddr ones_addr = { .mac = { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu } };

struct FrameControl {
    uint16_t ver:2;       // ESP LSB
    uint16_t type:2;
    uint16_t subtype:4;

    uint16_t toDS:1;
    uint16_t fromDS:1;
    uint16_t moreFrag:1;
    uint16_t retry:1;
    uint16_t powerMgmt:1;
    uint16_t moreData:1;
    uint16_t protFrame:1;
    uint16_t order:1;     // ESP MSB
} STRUCT_PACKED;

struct SeqCtl {
    uint16_t fragNum:4;
    uint16_t seqNum:12;
} STRUCT_PACKED;

struct TimeStamp {
    union {
        uint8_t u8[8];
        uint32_t u32[2];
        uint64_t u64;
    };
} STRUCT_PACKED;

// Beacons are sent periodically at a time called "Target Beacon Transmission
// Time (TBTT)" 1 TU = 1024 microseconds. Beacon interval =100 TU (100x 1024
// microseconds or 102.4 milliseconds)

struct MgmtBeacon {
    // ! timestamp is going to be in Network order we need to reverse
    TimeStamp timestamp;  // the number of microseconds the AP has been active.
    uint16_t beacon_int;  // 1 TU = 1024 microseconds.
    uint16_t capab_info;
    uint8_t  variable[];  // TLV
    // ssid     id=0, <len>, <Network Name>
    // rates ...
} STRUCT_PACKED;


struct MgmtProbeReq {
   uint8_t  variable [0];
} STRUCT_PACKED;

struct TLV {
    const uint8_t id;
    const uint8_t len;
    const uint8_t value[];
} STRUCT_PACKED;

struct LLC {
    const uint32_t ig:1;
    const uint32_t dsap:7;
    const uint32_t cr:1;
    const uint32_t ssap:7;
    const uint32_t frametype:2;
    const uint32_t ui:6;
    const uint32_t oc:24;
    const uint32_t type:16;
} STRUCT_PACKED;

struct QOS_CNTRL {
    const uint16_t tid:4;
    const uint16_t qos:1;
    const uint16_t ack:2;
    const uint16_t type:1;
    const uint16_t txop:8;
} STRUCT_PACKED;

constexpr uint16_t k_802_1x_authentication = (0x8E88u);

struct WiFiPktHdr {
    FrameControl fctl;
    uint16_t duration;
    union {
        MacAddr ra;
        MacAddr addr1;
    };
    union {
        MacAddr ta;
        MacAddr addr2;
    };
    union {
        MacAddr da;
        MacAddr addr3;
    };
    // unt16_t seqctl;
    SeqCtl seqctl;
    union {
        // uint8_t payload[];
        MgmtBeacon beacon;
        MgmtBeacon probe_resp;
        MgmtProbeReq probe_req;
        MacAddr addr4;
    };
} STRUCT_PACKED;

size_t getChannel();
uint32_t getFilter();
uint32_t begin_promiscuous(uint32_t c);
uint32_t begin_promiscuous(uint32_t c, uint32_t filter, uint32_t ctrl_filter);
void usbCdcEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

////////////////////////////////////////////////////////////////////////////////
//
#if 0
// At this time I do not need any of these byte swap functions
// Maybe delete later or just keep.
// __bswap_16 and __bswap_32 and __bswap_64 __builtin_bswap32
// #include <byteswap.h>
#if 1
inline uint64_t hton64(uint64_t u) { return __builtin_bswap64(u); }
inline uint64_t ntoh64(uint64_t u) { return __builtin_bswap64(u); }
inline uint32_t hton32(uint32_t u) { return __builtin_bswap32(u); }
inline uint32_t ntoh32(uint32_t u) { return __builtin_bswap32(u); }
inline uint16_t hton16(uint16_t u) { return __builtin_bswap16(u); }
inline uint16_t ntoh16(uint16_t u) { return __builtin_bswap16(u); }

inline uint64_t hton(uint64_t u) { return __builtin_bswap64(u); }
inline uint64_t ntoh(uint64_t u) { return __builtin_bswap64(u); }
inline uint32_t hton(uint32_t u) { return __builtin_bswap32(u); }
inline uint32_t ntoh(uint32_t u) { return __builtin_bswap32(u); }
// inline uint16_t hton(uint16_t u) { return __builtin_bswap16(u); }
// inline uint16_t ntoh(uint16_t u) { return __builtin_bswap16(u); }
#else
inline uint64_t hton64(uint64_t u) { return (u); }
inline uint64_t ntoh64(uint64_t u) { return (u); }
inline uint32_t hton32(uint32_t u) { return (u); }
inline uint32_t ntoh32(uint32_t u) { return (u); }
inline uint16_t hton16(uint16_t u) { return (u); }
inline uint16_t ntoh16(uint16_t u) { return (u); }
#endif
#endif

#endif  // WIFIPCAP_H
