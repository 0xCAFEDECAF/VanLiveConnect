#ifndef Config_h
#define Config_h
 
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

// Which type of packets will be printed on Serial?

//#define SELECTED_PACKETS VAN_PACKETS_COM2000_ETC
#define SELECTED_PACKETS VAN_PACKETS_HEAD_UNIT
//#define SELECTED_PACKETS VAN_PACKETS_SAT_NAV

#endif // Config_h
