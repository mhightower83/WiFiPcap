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

/*
   Serial PCAP - Takes logged WiFi packets and streams them out the USB CDC
   interface. A Newly opened inteface open gets a PCAP Header before the first
   PCAP Data Header followed by the Network packet.

   The use of stdin on Wireshark requires PCAP Version 2.4
     https://wiki.wireshark.org/CaptureSetup/Pipes.md
*/

#include "KConfig.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include "SerialPcap.h"
#include "WiFiPcap.h"
#include "Interlocks.h"

#ifndef USE_WIFIPCAP_FILTER_AP_SESSION
#define USE_WIFIPCAP_FILTER_AP_SESSION 0
#endif

#if ! ARDUINO_USB_CDC_ON_BOOT
USBCDC USBSerial(0);
#endif

static const char *TAG = "SerialPcap";
#if RELEASE_BUILD
#undef ESP_LOGI
#define ESP_LOGI(t, fmt, ...)
#endif

extern "C" {

#define WIFIPCAP_PAYLOAD_FCS_LEN               (4)
#define WIFIPCAP_PROCESS_PACKET_TIMEOUT_MS     (100)
#define WIFIPCAP_HP_PROCESS_PACKET_TIMEOUT_MS  (10)      // High Priority Task

#define USCLOCK32_ROLLOVER_SECONDS              (4294)    // 2^^32
#define USCLOCK32_ROLLOVER_MICROSECONDS         (967296)


struct TaskState {
    uint32_t is_running:1;
    uint32_t need_resync:1;
    uint32_t need_init:1;
    uint32_t dtr:1;
    uint32_t rts:1;
};

union UTaskState {
  uint32_t u32;
  TaskState b;
};


struct CustomFilters {
    bool badpkt;
    bool fcslen;
    bool session;
    size_t mcastlen; // 0, 1, 3, or 6
    MacAddr mcast;   // Multicast Address
    size_t moilen;   // 0 == None, 3 == OUI, 6 == MAC
    MacAddr moi;     // MAC Address Of Interest
};

CustomFilters __NOINIT_ATTR cust_fltr;

struct SerialTask {
    TaskState volatile state;
    uint32_t channel = 0;
    SERIAL_INF* volatile pcapSerial = NULL;
    TaskHandle_t volatile task = NULL;
    QueueHandle_t volatile work_queue = NULL;

    // Track time rollover, takes ~1.193046 hours
    // Also holds host GMT time of day used in the PCAP Packet Headers
    uint32_t timeseconds = 0;
    uint32_t timemicroseconds = 0;
    uint32_t last_microseconds = 0;
    uint32_t finish_host_time_sync = true;
    // SemaphoreHandle_t sem_task_over = NULL;
};

static SerialTask st;

////////////////////////////////////////////////////////////////////////////////
//
void reinit_serial(SerialTask *session) {
  // This appears to be needed to recover from a failed start/sync
#if ARDUINO_USB_MODE
  // HWCDC
  //C Calling Serial.end() creates a boot loop for ARDUINO_USB_MODE
  //C if (*session->pcapSerial) session->pcapSerial->end();
  if (*session->pcapSerial) session->pcapSerial->end();
  session->pcapSerial->begin();
  session->pcapSerial->setTxBufferSize(CONFIG_WIFIPCAP_SERIAL_TX_BUFFER_SIZE); // Only HWCDC
#else
  // USBCDC
  if (*session->pcapSerial) session->pcapSerial->end();
  session->pcapSerial->begin();
  // USBCDC does not have a setTxBufferSize method.

  // For now, Hardware Serial is not supported
  #if 0
  session->pcapSerial->setTxBufferSize(CONFIG_WIFIPCAP_SERIAL_TX_BUFFER_SIZE);
  session->pcapSerial->begin(CONFIG_WIFIPCAP_SERIAL_SPEED, SERIAL_8N1);
  #endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Of data pairs (eg. 'U' and 'u'), major value before minor . Major values are
// in caps and minor in lower. Configuration is finished with a 'X' for execute.
//

void printSettings(SerialTask *session, const char *title) {
    session->pcapSerial->printf("%s\n", title);
    session->pcapSerial->printf("  Channel: %u\n", getChannel());
    session->pcapSerial->printf("  %s 0x%08X\n", "filter:", getFilter());

    if (cust_fltr.badpkt) session->pcapSerial->printf("  %s\n", "Keep WIFI_PROMIS_FILTER_MASK_FCSFAIL");
    if (cust_fltr.fcslen) session->pcapSerial->printf("  %s\n", "k_filter_custom_fcslen");
    if (cust_fltr.session) session->pcapSerial->printf("  %s\n", "k_filter_custom_session");
    if (cust_fltr.mcastlen) {
        session->pcapSerial->printf("  %s: '", "multicast");
        session->pcapSerial->printf("%02X", cust_fltr.mcast.mac[0]);
        for (size_t i = 1; i < cust_fltr.mcastlen; i++)
            session->pcapSerial->printf(":%02X", cust_fltr.mcast.mac[i]);
        session->pcapSerial->printf("'\n");
    }
    if (cust_fltr.moilen) {
        session->pcapSerial->printf("  %s: '", "unicast");
        session->pcapSerial->printf("%02X", cust_fltr.moi.mac[0]);
        for (size_t i = 1; i < cust_fltr.moilen; i++)
            session->pcapSerial->printf(":%02X", cust_fltr.moi.mac[i]);
        session->pcapSerial->printf("'\n");
    }
}

size_t parseInt2Array(uint8_t* array, int32_t* mac, SerialTask *session) {
    *mac = session->pcapSerial->parseInt();
    if (0 >= *mac) {
        array[0] = array[1] = array[2] = 0;
        return 0;
    }
    array[0] = (uint8_t)(*mac >> 16);
    array[1] = (uint8_t)(*mac >> 8);
    array[2] = (uint8_t)*mac;
    return 3u;
}

esp_err_t hostDialog(SerialTask *session, int& channel, uint32_t& filter) {
    ESP_LOGI(TAG, "Say Hello to Host");
    // Be helpful, tell them where to download the script from
    session->pcapSerial->printf("\nUse with script:\n  https://raw.githubusercontent.com/mhightower83/WiFiPcap/extras/esp32shark.py\n");
    // First, say Hello to the python script
    session->pcapSerial->printf("\n<<SerialPcap>>\n");

    session->timeseconds = 0;
    session->timemicroseconds = 0;
    session->finish_host_time_sync = true;

    ESP_LOGI(TAG, "Wait for Host Sync");
    uint32_t start = millis();
    while (0 >= session->pcapSerial->available()) {
        uint32_t now = millis();
        if (now - start > 500) {
            ESP_LOGE(TAG, "Serial Read Timeout");
            return ESP_ERR_TIMEOUT;
        }
        delay(1);
    }

    channel = getChannel();
    filter = 0;
    int c = session->pcapSerial->read();
    for (; '\n' != c && 0 < c; c = session->pcapSerial->read()) {
        if ('C' ==  c) {
            int32_t val = session->pcapSerial->parseInt();
            if (0 < val && maxChannel >= val) channel = val;
        } else
        if ('F' ==  c) {
            filter = session->pcapSerial->parseInt();
            filter <<= 16u;
        } else
        if ('f' ==  c) {
            filter |= session->pcapSerial->parseInt();
        } else
        if ('U' ==  c) {  // Unicast
            int32_t mac;
            cust_fltr.moilen = parseInt2Array(&cust_fltr.moi.mac[0], &mac, session);
        } else
        if ('u' ==  c) {
            int32_t mac;
            size_t len = parseInt2Array(&cust_fltr.moi.mac[3], &mac, session);
            if (cust_fltr.moilen) cust_fltr.moilen += len;
        } else
        if ('M' == c) {  // Multicast
            int32_t mac;
            size_t len = parseInt2Array(&cust_fltr.mcast.mac[0], &mac, session);
            cust_fltr.mcastlen = ((1<<16) == mac) ? 1 : len; // 1, assume Pass all multicast packets
        } else
        if ('m' == c) {
            int32_t mac;
            size_t len = parseInt2Array(&cust_fltr.mcast.mac[3], &mac, session);
            if (1 == cust_fltr.mcastlen && len) cust_fltr.mcastlen += 2;  // correct bad assumption
            if (3 == cust_fltr.mcastlen) cust_fltr.mcastlen += len;
        } else
        if ('G' == c) {
            int32_t i = session->pcapSerial->parseInt();
            if (i > 0) {
                session->timeseconds = i;
            } else {
                ESP_LOGE(TAG, "Missing Host time.");
            }
        } else
        if ('g' == c) {
            int32_t i = session->pcapSerial->parseInt();
            if (i > 0 && 1000000u > i) {
                session->timemicroseconds = i;
            } else {
                ESP_LOGE(TAG, "Malformed Host time.");
            }
        } else
        if ('P' == c) {
            printSettings(session, "Current Config Settings");
        } else
        if ('X' == c) {
            // filter == 0, Carry forward previous filter
            if (filter) {
                cust_fltr.badpkt = (0 != (WIFI_PROMIS_FILTER_MASK_FCSFAIL & filter));
                cust_fltr.fcslen = (0 != (k_filter_custom_fcslen & filter));
                cust_fltr.session = (0 != (k_filter_custom_session & filter));
            }
            if (0 == cust_fltr.moilen) {
                cust_fltr.mcastlen = 0;
            }
            if (cust_fltr.session) {
                filter |= WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
            }
            // remove custom bits - invalid SDK filter bit
            filter &= ~(k_filter_custom_fcslen | k_filter_custom_session);
            printSettings(session, "Final Config Settings");
            session->pcapSerial->printf("<<PASSTHROUGH>>\n");
            ESP_LOGI(TAG, "Host Sync Complete");
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Unkown config ID: '%c'", c);
        }
    }
    ESP_LOGE(TAG, "Missing 'X' at the end of config");
    return ESP_ERR_TIMEOUT;
}


/*
  To access shared memory updates I use interlocked_read and
  interlocked_compare_exchange. The do {} while( ...
  interlocked_compare_exchange calls) are at setup and error recover thus do
  not get called a lot. This allows the frequent use of interlocked_read to
  quickly and safely access the state.

  xSemaphoreGive/xSemaphoreTake may be an alternative; however, I am use to
  using interlocked_* approach and it works.

  I expect the do {} while( ... interlocked_compare_exchange calls) to loop at
  most twice. Which I think is better than having a delay time with
  xSemaphoreGive or xSemaphoreTake.
*/
////////////////////////////////////////////////////////////////////////////////
// Send PCAP File Header info
esp_err_t pcap_serial_start(SerialTask *session, pcap_link_type_t link_type) {

    union UTaskState state;
    state.u32 = interlocked_read((volatile uint32_t*)&session->state);
    if (state.b.is_running && !state.b.need_resync) return ESP_OK;

    ESP_LOGI(TAG, "pcap_serial_start");
    if (state.b.need_init) {
        reinit_serial(session);
        union UTaskState old_state;
        do {
            old_state.u32 = (uint32_t)interlocked_read((volatile uint32_t*)&session->state);
            state = old_state;
            state.b.need_init = false;
        } while (false == interlocked_compare_exchange((volatile uint32_t*)&session->state, old_state.u32, state.u32));
    }

    // Wait for the host side to start Serial APP, etc
    state.u32 = interlocked_read((volatile uint32_t*)&session->state);
    if (!state.b.dtr) {
        // Not yet ready
        ESP_LOGI(TAG, "Host not yet ready DTR: %s, %sUSBSerial", (state.b.dtr) ? "HIGH" : "LOW", (*session->pcapSerial) ? "" : "!");
        return ESP_FAIL;
    }
    if (! *session->pcapSerial) {
        //?? I thought this use to work and now it does not ??
        // There might be a bug in USBCDC.cpp
        // See comments for serial_pcap_notifyDtrRts(). This maybe an issue with
        // the wierd rts/dtr reset & flash programming option. Which never
        // worked to me.
        //
        // We now rely on state.b.dtr as ready indicator
        ESP_LOGI(TAG, "Host status DTR: %s, %sUSBSerial", (state.b.dtr) ? "HIGH" : "LOW", (*session->pcapSerial) ? "" : "!");
    }

    ESP_LOGI(TAG, "Empty RX FIFO");
    // Clear Serial RX FIFO
    while (0 < session->pcapSerial->available()) session->pcapSerial->read();

    int channel;
    uint32_t filter;
    // Poll host for the Promiscuous Configuration
    if (ESP_OK != hostDialog(session, channel, filter)) {
        // Host not ready
        return ESP_FAIL;
    }

    begin_promiscuous(channel, filter, filter);

    // Write Pcap File header - About PCAP_MAGIC, The decoder will use it to
    // detect if byte swapping is needed when interpreting the results. No need
    // for extra care in constructing byteswapped headers.
    PcapFileHeader header = {
        .magic = PCAP_MAGIC,                      // 0xA1B2C3D4 PCAP Magic value
        .major = PCAP_DEFAULT_VERSION_MAJOR,      // 2.4
        .minor = PCAP_DEFAULT_VERSION_MINOR,      //
        .zone  = PCAP_DEFAULT_TIME_ZONE_GMT,      // Most implementations set tp 0
        .sigfigs = 0,                             // accuracy of timestamps, ditto above
        .snaplen = PCAP_MAX_CAPTURE_PACKET_SIZE,  // MAX length of captured packets, in octets
        .link_type = link_type
    };

    size_t real_write = session->pcapSerial->write((const uint8_t *)&header, sizeof(header));
    if (sizeof(header) == real_write) {
        // All is good. We can now forward packets to the Serial interface with
        // a pcap packet header and the script will pass it on to Wireshark.
        reset_dropped_count();
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Write PCAP File Header failed!");
    union UTaskState old_state;
    do {
        old_state.u32 = interlocked_read((volatile uint32_t*)&session->state);
        state = old_state;
        state.b.need_resync = true;
        state.b.need_init = true;
    } while (false == interlocked_compare_exchange((volatile uint32_t*)&session->state, old_state.u32, state.u32));
    return ESP_FAIL;
}

////////////////////////////////////////////////////////////////////////////////
//
/*
  to handshake through "reboot_enable" behavior in USBCDC.cpp
  For a resync, host should
   1) drop DTR, event post disconnected advance to state  CDC_LINE_1
   2) drop RTS, back to state CDC_LINE_IDLE

  For connecting back with both DTR and RTS false.
   1) Raise DTR
   2) Raise RTS
*/
void serial_pcap_notifyDtrRts(bool dtr, bool rts) {
    SerialTask *session = &st;
    /*
      For USBCDC, false DTR, connected, and CDC_LINE_IDLE will change
      connected to false;
    */
    union UTaskState old_state, state;
    do {
        old_state.u32 = interlocked_read((volatile uint32_t*)&session->state);
        state = old_state;
        state.b.dtr = dtr;
        state.b.rts = rts;
        if (false == old_state.b.dtr && true == dtr && state.b.is_running) {
            state.b.need_resync = state.b.need_init = true;
        }
    } while (false == interlocked_compare_exchange((volatile uint32_t*)&session->state, old_state.u32, state.u32));
}

///////////////////////////////////////////////////////////////////////////////
// Handles Host time sync up, 32 bit counter rollovers, and finalzing pcap
// packet header timestamp.
static inline void pcap_time_sync(SerialTask *session, WiFiPcap *wpcap) {
    if (session->finish_host_time_sync) {
        session->finish_host_time_sync = false;
        // assume this to be ESP32 system time
        const uint32_t systime = wpcap->pcap_header.microseconds;
        const uint32_t seconds = systime / 1000000u;
        const uint32_t microseconds = systime % 1000000u;

        session->timeseconds -= seconds;
        if (microseconds > session->timemicroseconds) {
            session->timeseconds--;
            session->timemicroseconds += 1000000u;
        }
        session->timemicroseconds -= microseconds;
        session->last_microseconds = systime;
    } else
    if (session->last_microseconds > wpcap->pcap_header.microseconds) {
        // catch 32-bit register rollover and perform carry
        // For this logic to work we must receive at least one packet every
        // 1.19 hours. Unless some really tight prefilters are used, this is
        // no expected to be an issue.
        session->timeseconds      += USCLOCK32_ROLLOVER_SECONDS;
        session->timemicroseconds += USCLOCK32_ROLLOVER_MICROSECONDS;
        if (1000000u <= session->timemicroseconds) {
            session->timeseconds++;
            session->timemicroseconds -= 1000000u;
        }
    }
    session->last_microseconds      = wpcap->pcap_header.microseconds;

    // Finish defered processing of timestamp in this non-critical path.
    wpcap->pcap_header.seconds      = wpcap->pcap_header.microseconds / 1000000u;
    wpcap->pcap_header.microseconds = wpcap->pcap_header.microseconds % 1000000u;

    // Add in system time correction - assumes GMT
    wpcap->pcap_header.seconds      += session->timeseconds;
    wpcap->pcap_header.microseconds += session->timemicroseconds;
    if (1000000u <= wpcap->pcap_header.microseconds) {
        wpcap->pcap_header.seconds  += 1;
        wpcap->pcap_header.microseconds -= 1000000u;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
static void serial_task(void *parameters) {
    SerialTask *session = (SerialTask *)parameters;
    WiFiPcap *wpcap = NULL;

    ESP_LOGI(TAG, "Task Started");
    union UTaskState state;
    state.u32 = interlocked_read((volatile uint32_t*)&session->state);

    if (state.b.is_running) {
        ESP_LOGE(TAG, "Task already running. Exiting ...");
        vTaskDelete(NULL);
    }

    union UTaskState old_state;
    do {
        old_state.u32 = interlocked_read((volatile uint32_t*)&session->state);
        state = old_state;
        state.b.is_running = true;
        state.b.need_resync = true;
        state.b.need_init = true;
    } while (false == interlocked_compare_exchange((volatile uint32_t*)&session->state, old_state.u32, state.u32));

    while (state.b.is_running) {
        // Get a captured packet from the queue
        if (pdTRUE != xQueueReceive(session->work_queue, &wpcap, pdMS_TO_TICKS(WIFIPCAP_PROCESS_PACKET_TIMEOUT_MS))) {
            wpcap = NULL;
        }
        state.u32 = interlocked_read((volatile uint32_t*)&session->state);
        bool need_resync = state.b.need_resync;
        while (need_resync) {
            if (wpcap) {
                free(wpcap);
                wpcap = NULL;
            }
            need_resync = (ESP_OK != pcap_serial_start(session, PCAP_LINK_TYPE_802_11));
            // Clear queue so we can get time synced properly with host
            while (pdTRUE == xQueueReceive(session->work_queue, &wpcap, 0)) {
                free(wpcap);
            }
            wpcap = NULL;

            if (need_resync) {
                delay(WIFIPCAP_PROCESS_PACKET_TIMEOUT_MS);
                // state = (TaskState)interlocked_read((volatile void**)&session->state);
            } else {
                do {
                    old_state.u32 = interlocked_read((volatile uint32_t*)&session->state);
                    state = old_state;
                    state.b.need_resync = !state.b.dtr;
                } while (false == interlocked_compare_exchange((volatile uint32_t*)&session->state, old_state.u32, state.u32));
            }
        }
        if (NULL == wpcap) continue;

        pcap_time_sync(session, wpcap);

        size_t total_length = offsetof(struct WiFiPcap, payload) + wpcap->pcap_header.capture_length;
        // We expect Serial.write() to block - don't expect to be exposed to stream timeout
        ssize_t wrote = session->pcapSerial->write((const uint8_t*)wpcap, total_length);
        if (wrote != total_length) {
            // This is the path taken when Wireshark exits and python script closes.
            //
            // How best to resync with Wireshark?
            // Maybe Serial.end() and start resync logic
            // Use need_resync/need_init for now.
            do {
                old_state.u32 = interlocked_read((volatile uint32_t*)&session->state);
                state = old_state;
                state.b.need_resync = true;
                state.b.need_init = true;
            } while (false == interlocked_compare_exchange((volatile uint32_t*)&session->state, old_state.u32, state.u32));
            if (state.b.dtr) {
                ESP_LOGE(TAG, "Write PCAP Packet failed!");
            } else {
                ESP_LOGE(TAG, "Host has disconnected!");  // maybe => ESP_LOGI
            }
            delay(1000);
            // TODO: Review TX timeout possible issues
        }
        free(wpcap);
    }
    // session->need_resync = false;
    session->task = NULL;
    do {
        old_state.u32 = interlocked_read((volatile uint32_t*)&session->state);
        state = old_state;
        state.b.need_resync = false;
        state.b.need_init = false;        // assume an idle stance
    } while (false == interlocked_compare_exchange((volatile uint32_t*)&session->state, old_state.u32, state.u32));

    // Drain queue and free allocations
    // Use WIFIPCAP_PROCESS_PACKET_TIMEOUT_MS to allow any inprogress
    // serial_pcap_cb()/xQueueSend to finish
    while (pdTRUE == xQueueReceive(session->work_queue, &wpcap, WIFIPCAP_PROCESS_PACKET_TIMEOUT_MS)) {
        free(wpcap);
    }
    // At this time, we never stop the task. So, this path is never taken.
    // Re-evaluate atomics when/if this changes
    // QueueHandle_t work_queue = interlocked_read((volatile void**)&session->work_queue);
    // interlocked_write(&session->work_queue, NULL);

    vQueueDelete(session->work_queue);
    session->work_queue = NULL;

    ESP_LOGE(TAG, "Task stopped!");
    vTaskDelete(NULL);
}


///////////////////////////////////////////////////////////////////////////////
//
#pragma GCC push_options
#pragma GCC optimize("Ofast")
esp_err_t serial_pcap_cb(void *recv_buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *snoop = (wifi_promiscuous_pkt_t *)recv_buf;
    SerialTask *session = &st;

    union UTaskState state;
    state.u32 = interlocked_read((volatile uint32_t*)&session->state);
    if (!state.b.is_running) return ESP_ERR_INVALID_STATE;

    // Skip error state packets - does this include with FCS Errors ??
    // rx_ctrl.rx_state is underdocumented. I assume it would be set for errors
    // other than fcsfail. Like runt packets, jumbo packets, DMA error, etc.
    // With WIFI_PROMIS_FILTER_MASK_FCSFAIL set, we keep all bad packets.
    if (cust_fltr.badpkt || 0 == snoop->rx_ctrl.rx_state) {

        // Apply prescreen filters
        if (cust_fltr.session) {
            const WiFiPktHdr* const wh = (WiFiPktHdr*)snoop->payload;
            // These seem to work for limiting capture of a TCP session
            // Note, while Wireshark is logging each side needs to authenticate
            // with the AP for decription to work.
            if (WIFI_PKT_MGMT == type ) {
                if (WLAN_FC_STYPE_BEACON     == wh->fctl.subtype) return ESP_OK;
                if (WLAN_FC_STYPE_PROBE_REQ  == wh->fctl.subtype) return ESP_OK;
                if (WLAN_FC_STYPE_PROBE_RESP == wh->fctl.subtype) return ESP_OK;
            }
            if (WIFI_PKT_DATA == type && (0x04u & wh->fctl.subtype)) return ESP_OK;
        }

        if (cust_fltr.moilen) {
            WiFiPktHdr *pkt = (WiFiPktHdr*)snoop->payload;
            do {
                if (cust_fltr.mcastlen) {
                    if (1 == cust_fltr.mcastlen) {
                        // Keep any broadcast response
                        if ((!pkt->fctl.toDS && (1u & pkt->ra.mac[0])) ||
                            ( pkt->fctl.toDS && (1u & pkt->addr3.mac[0])) ) {
                            continue;
                        }
                    } else {
                      // Keep selective broadcast
                      size_t len = cust_fltr.mcastlen;
                      uint8_t *moi = cust_fltr.mcast.mac;
                      if ((!pkt->fctl.toDS && 0 == memcmp(pkt->ra.mac,    moi, len)) ||
                          ( pkt->fctl.toDS && 0 == memcmp(pkt->addr3.mac, moi, len))) {
                          continue;
                      }
                    }
                }
                // By using 3 or 6 bytes, we switch from an OUI block to a full MAC address
                size_t len = cust_fltr.moilen;
                uint8_t *moi = cust_fltr.moi.mac;
                if (6 == len) {
                    do {  // Log packet on interesting Source Address or Destination Address
                        if (!pkt->fctl.toDS   && pkt->ra.mac[5] == moi[5] && 0 == memcmp(pkt->ra.mac, moi, 5)) continue;
                        if (!pkt->fctl.fromDS && pkt->ta.mac[5] == moi[5] && 0 == memcmp(pkt->ta.mac, moi, 5)) continue;
                        if ((pkt->fctl.toDS || pkt->fctl.fromDS)
                                              && pkt->addr3.mac[5] == moi[5] && 0 == memcmp(pkt->addr3.mac, moi, 5)) continue;
                        if ( pkt->fctl.toDS && pkt->fctl.fromDS
                                              && pkt->addr4.mac[5] == moi[5] && 0 == memcmp(pkt->addr4.mac, moi, 5)) continue;
                        return ESP_OK;
                    } while (false);
                } else
                if (3 == len) {
                    do {  // Log packet on interesting Source Address or Destination Address
                        if (!pkt->fctl.toDS   && pkt->ra.mac[0] == moi[0] && pkt->ra.mac[1] == moi[1] && pkt->ra.mac[2] == moi[2] ) continue;
                        if (!pkt->fctl.fromDS && pkt->ta.mac[0] == moi[0] && pkt->ta.mac[1] == moi[1] && pkt->ta.mac[2] == moi[2]) continue;
                        if ((pkt->fctl.toDS || pkt->fctl.fromDS)
                                              && pkt->addr3.mac[0] == moi[0] && pkt->addr3.mac[1] == moi[1] && pkt->addr3.mac[2] == moi[2]) continue;
                        if ( pkt->fctl.toDS && pkt->fctl.fromDS
                                              && pkt->addr4.mac[0] == moi[0] && pkt->addr4.mac[1] == moi[1] && pkt->addr4.mac[2] == moi[2]) continue;
                        return ESP_OK;
                    } while (false);
                }
#if 0
                else if (len) {
                    do {  // Log packet on interesting Source Address or Destination Address
                        if (!pkt->fctl.toDS   && 0 == memcmp(pkt->ra.mac,    moi, len)) continue;
                        if (!pkt->fctl.fromDS && 0 == memcmp(pkt->ta.mac,    moi, len)) continue;
                        if ((pkt->fctl.toDS || pkt->fctl.fromDS)
                                              && 0 == memcmp(pkt->addr3.mac, moi, len)) continue;
                        if ( pkt->fctl.toDS && pkt->fctl.fromDS
                                              && 0 == memcmp(pkt->addr4.mac, moi, len)) continue;
                        return ESP_OK;
                    } while (false);
                }
#endif
            } while (false);
        }

        ssize_t length = snoop->rx_ctrl.sig_len;
        if (! cust_fltr.fcslen) {
            length -= WIFIPCAP_PAYLOAD_FCS_LEN;
        }
        ssize_t keepLength = length;
        if (keepLength > PCAP_MAX_CAPTURE_PACKET_SIZE) keepLength = PCAP_MAX_CAPTURE_PACKET_SIZE;
        if (keepLength > 0) {
            // This may need to use PSRAM
            // Use work_queue size as a limiter on total memory allocated.wpcap->payload / 1000000u;
            WiFiPcap *wpcap = (WiFiPcap*)malloc(keepLength + sizeof(WiFiPcap));
            if (wpcap) {
                // Make a copy of received packet
                memcpy(wpcap->payload, snoop->payload, keepLength);
                /*
                  Prepare pcap packet header
                */
                // Critical path, defer divides and timestamp corrections till later
                //   seconds = snoop->rx_ctrl.timestamp / 1000000u;
                //   microseconds = snoop->rx_ctrl.timestamp % 1000000u;
                wpcap->pcap_header.microseconds = snoop->rx_ctrl.timestamp;
                wpcap->pcap_header.capture_length = keepLength;
                wpcap->pcap_header.packet_length = length;

                // Queue Wireshark/pcap ready packet
                // Allow brief blocking - WIFIPCAP_HP_PROCESS_PACKET_TIMEOUT_MS
                //   * so xQueueReceive can finish and we can avoid dropping
                //     the packet
                //   * short enough to avoid overflow in the SDK's WiFi RX
                //     calling path to serial_pcap_cb().

                // QueueHandle_t work_queue = (QueueHandle_t)interlocked_read((void*)&session->work_queue);
                // if (NULL != work_queue && pdTRUE != xQueueSend(work_queue, &wpcap, pdMS_TO_TICKS(WIFIPCAP_HP_PROCESS_PACKET_TIMEOUT_MS))) {
                if (pdTRUE != xQueueSend(session->work_queue, &wpcap, pdMS_TO_TICKS(WIFIPCAP_HP_PROCESS_PACKET_TIMEOUT_MS))) {
                    // ESP_LOGE(TAG, "snoop work queue full");
                    free(wpcap);
                    return ESP_ERR_TIMEOUT;
                }
            } else {
                return ESP_ERR_NO_MEM;
            }
        }
    }
    return ESP_OK;
}
#pragma GCC pop_options

////////////////////////////////////////////////////////////////////////////////
//
/*
  setup captured packet queue and worker thread
*/
esp_err_t serial_pcap_start(SERIAL_INF* pcapSerial, bool init_custom_filter) {
    SerialTask *session = &st;

    // if (session->is_running) return ESP_OK;
    // TaskState state = (TaskState)interlocked_read((volatile void**)&session->state);
    // if (!state.is_running) return 1;
    HWSerial.printf("work_queue: 0x%08X\n", (uint32_t)session->work_queue);
    void* val = interlocked_read((volatile void**)&session->work_queue);
    HWSerial.printf("work_queue: 0x%08X\n", (uint32_t)val);

    if (interlocked_read((volatile void**)&session->work_queue)) return ESP_FAIL;

    // init state
    session->state.is_running = false;
    session->state.need_resync = false;
    session->state.need_init = true;
    session->state.dtr = false;
    session->state.rts = false;

    if (init_custom_filter) {
        cust_fltr.badpkt = false;
        cust_fltr.fcslen = false;
        cust_fltr.session = (USE_WIFIPCAP_FILTER_AP_SESSION) ? true : false;
        cust_fltr.mcastlen = 0;
        cust_fltr.moilen = 0;
    }

    if (NULL == pcapSerial) {
        ESP_LOGE(TAG, "NULL Pcap Serial");
        return ESP_FAIL;
    }
    session->pcapSerial = pcapSerial;
    session->work_queue = xQueueCreate(CONFIG_WIFIPCAP_WORK_QUEUE_LEN, sizeof(WiFiPcap*));

    if (NULL == session->work_queue) {
        ESP_LOGE(TAG, "create work queue failed");
        return ESP_FAIL;
    }

    if (pdPASS == xTaskCreatePinnedToCore(
        serial_task,                     // TaskFunction_t, Function to implement the task
        "SerialTask",                    // char *, Task Name
        CONFIG_WIFIPCAP_TASK_STACK_SIZE, // uint32_t, Stack size in bytes (4 byte increments)
        session,                         // void *, Task input parameter
        CONFIG_WIFIPCAP_TASK_PRIORITY,   // 2 - UBaseType_t , Priority of the task
        (void**)&session->task,          // TaskHandle_t *, Task handle
        // PRO_CPU_NUM)                    // BaseType_t, Core where the task should run
        APP_CPU_NUM)                     // BaseType_t, Core where the task should run
        ) {
        //
        // Linux appears to drop key-strokes when the USB CDC is busy with
        // Wireshark.
        //
        // Ideally we want to run on the opposite core to the SDK, splitting
        // the burdon of packet handling.
        //
        // The task will set "is_running" at startup
        ESP_LOGI(TAG, "Create Task Success");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Create Task Failed!");
    delay(100);

    // session->pcapSerial->end();  // These calls appear to cause a crash
    session->pcapSerial = NULL;

    vQueueDelete(session->work_queue);
    session->work_queue = NULL;

    return ESP_FAIL;
}




};
