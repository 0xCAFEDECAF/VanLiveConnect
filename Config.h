#ifndef Config_h
#define Config_h
 
#define WIFI_AP_MODE

#ifdef WIFI_AP_MODE

#define WIFI_SSID "PSA display AP"
#define WIFI_PASSWORD "99999999"

#else

#define WIFI_SSID "MyCar"  // Choose yours
#define WIFI_PASSWORD "WiFiPass"  // Fill in yours

inline void WifiConfig()
{
    // Write your Wi-Fi config stuff here, like setting IP stuff, e.g.:
    // IPAddress ip(192, 168, 1, 2);
    // IPAddress gateway(192, 168, 1, 1);
    // IPAddress subnet(255, 255, 255, 0);
    // IPAddress dns(192, 168, 1, 1);
    // WiFi.config(ip, gateway, subnet, dns);
} // WifiConfig

#endif // WIFI_AP_MODE

#endif // Config_h
