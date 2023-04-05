/*
  These defines are relativly static. They are not expected to change for a
  specific board build. In contrast with `WiFiPcap.ino.globals.h`, which is
  the place for global build options. (Or build_opt.h)
*/
#ifndef KCONFIG_H
#define KCONFIG_H

/*
    CONFIG_WIFIPCAP_WORK_QUEUE_LEN

    int "Number of packet entries in the work queue"
    default 128
    help
        The filter callback function should not do a lot of work, the time
        consuming IO operations are defered to the SerialPcap task on a
        different CPU if possible.

        The SerialPcap task gets pointers to captured packets from the queue and
        passes them on to the host. Here you specify the length of that queue.
*/
#define CONFIG_WIFIPCAP_WORK_QUEUE_LEN 128u


/*
    CONFIG_WIFIPCAP_TASK_STACK_SIZE

    int "Stack size of SerialPcap task"
    default 4096
    help
        Stack size of SerialPcap task.
        TODO: Need to measure the amount of stack space needed.
*/
#define CONFIG_WIFIPCAP_TASK_STACK_SIZE 4096u


/*
    CONFIG_WIFIPCAP_TASK_PRIORITY

    int "Priority of SerialPcap task"
    default 2
    help
        Priority of SerialPcap task.
*/
#define CONFIG_WIFIPCAP_TASK_PRIORITY 2u


/*
    CONFIG_WIFIPCAP_SERIAL_SPEED

    int "Serial line speed to host"
    default 921600
    help
        Serial line speed to host. Provided to but not used by the USB CDC
        interface.
*/
#define CONFIG_WIFIPCAP_SERIAL_SPEED 921600


/*
    CONFIG_WIFIPCAP_CHANNEL

    int "Default WiFi Channel selected at boot"
    default 2
    help
        WiFi Channel selected at boot. Can range from 1 to 11, 13, or 14
        region dependent.
*/
#define CONFIG_WIFIPCAP_CHANNEL 6u


/*
    CONFIG_WIFIPCAP_CHANNEL_MAX

    int "Maximum WiFi Channel available in your region."
    default 11
    help
        Maximum WiFi Channel allowed for your local region.
        Can range from 11, 13, or 14. Local or regional government regulated.
*/
// #define CONFIG_WIFIPCAP_CHANNEL_MAX 13u   // Europe
// #define CONFIG_WIFIPCAP_CHANNEL_MAX 13u   // North America, low power option
// #define CONFIG_WIFIPCAP_CHANNEL_MAX 11u   // North America

// Since we are a passive receiver (no TX) accept full WiFi channel range
// Let the script place channel selection limits.
#define CONFIG_WIFIPCAP_CHANNEL_MAX 14u   // ??


/*
    CONFIG_WIFIPCAP_SERIAL_TX_BUFFER_SIZE

    int "Serial TX buffer size."
    default 2*1024
    help
        Used by SerialPcap to configure TX buffer size to host.
*/
#define CONFIG_WIFIPCAP_SERIAL_TX_BUFFER_SIZE (2*1024)

/* Document
  !) Not useful, ESP32 does not appear to give us the FCS. It just includes the
  size of FCS in the packet length :(

  It could be it works, we just cannot find an explaination in the sea of knowledge.

  Added k_filter_custom_fcslen - can test more later

  Pass FCS on to Wireshark
  Wireshark may need setting change
      Edit->Preferences->Protocols->Ethernet->"Assume Packet has FCS"
      https://osqa-ask.wireshark.org/questions/6293/fcs-check-is-not-displayed/
// #define CONFIG_WIFIPCAP_INCLUDE_FCS 1
 */

#endif
