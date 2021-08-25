#ifndef Config_h
#define Config_h

#include <ESP8266WiFi.h>

// -----
// Wi-Fi

// Define to use Wi-Fi "access point" mode. Comment out to use Wi-Fi station (client) mode
#define WIFI_AP_MODE

#ifdef WIFI_AP_MODE

#define WIFI_SSID "PSA display AP"

// Define to set password on access point
//#define WIFI_PASSWORD "99999999"

#define AP_IP 172,217,28,1

#else

#define WIFI_SSID "WiFiHotspot"  // Choose yours
#define WIFI_PASSWORD "WiFiPass"  // Fill in yours

inline void WifiConfig()
{
    // Write your Wi-Fi config stuff here, like setting IP stuff, e.g. for an Android Wi-Fi hotspot:
    IPAddress ip(192, 168, 43, 2);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(192, 168, 43, 1);
    WiFi.config(ip, gateway, subnet, dns);

    // Note: if you don't set a fixed IP address you will normally get one from the DHCP server, which may vary.
} // WifiConfig

#endif // WIFI_AP_MODE

// Comment out to use the mDNS responder
//#define USE_MDNS

// -----
// Infrared receiver

// Choose by uncommenting one of these #defines below (or define your own)
//#define IR_TSOP48XX
#define IR_TSOP312XX

// TSOP48XX
#ifdef IR_TSOP48XX

// IR receiver data pin
#define IR_RECV_PIN D5

// Using D7 as VCC and D6 as ground pin for the IR receiver. Should be possible with e.g. the
// TSOP4838 IR receiver as it typically uses only 0.7 mA.
#define IR_VCC D7
#define IR_GND D6

#endif // IR_TSOP48XX

// TSOP312XX
#ifdef IR_TSOP312XX

// IR receiver data pin
#define IR_RECV_PIN D7

// Using D7 as VCC and D6 as ground pin for the IR receiver. Should be possible with e.g. the
// TSOP31238 IR receiver as it typically uses only 0.35 mA.
#define IR_VCC D5
#define IR_GND D0

#endif // IR_TSOP312XX

// -----
// Debugging

// Uncomment to see JSON buffers printed on the Serial port
#define PRINT_JSON_BUFFERS_ON_SERIAL

// Which type of packets will be printed on Serial?

#define SELECTED_PACKETS VAN_PACKETS_ALL
//#define SELECTED_PACKETS VAN_PACKETS_COM2000_ETC
//#define SELECTED_PACKETS VAN_PACKETS_HEAD_UNIT
//#define SELECTED_PACKETS VAN_PACKETS_SAT_NAV
//#define SELECTED_PACKETS VAN_PACKETS_NONE

#endif // Config_h
