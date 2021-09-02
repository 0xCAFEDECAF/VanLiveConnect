
#include "Config.h"

const char* GetHostname()
{
    return "VanLiveConnect";
} // GetHostname

void SetupWifi()
{
    WiFi.hostname(GetHostname());

#ifdef WIFI_AP_MODE

    Serial.printf_P(PSTR("Setting up captive portal on Wi-Fi access point '%s' ..."), WIFI_SSID);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
#ifdef WIFI_PASSWORD
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
#else
    WiFi.softAP(WIFI_SSID);
#endif

#else  // ! WIFI_AP_MODE

    Serial.printf_P(PSTR("Connecting to Wi-Fi SSID '%s'\n"), WIFI_SSID);

    WifiConfig();

    // TODO - does this decrease the jitter on the bit timings?
    wifi_set_sleep_type(NONE_SLEEP_T);

    WiFi.mode(WIFI_STA);  // Otherwise it may be in WIFI_AP_STA mode, broadcasting an SSID like AI_THINKER_XXXXXX
    WiFi.disconnect();  // After reset via HW button sometimes cannot seem to reconnect without this
    WiFi.persistent(false);
    WiFi.setAutoConnect(true);

    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
    WiFi.setOutputPower(20.5);

    // TODO - using Wi-Fi, unfortunately, has a detrimental effect on the packet CRC error rate. It will rise from
    // around 0.006% up to 0.1% or more. Underlying cause is timing failures due to Wi-Fi causing varying interrupt
    // latency. Not sure how to tackle this.
#ifdef WIFI_PASSWORD
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#else
    WiFi.begin(WIFI_SSID);
#endif

#endif // WIFI_AP_MODE

    delay(1);
} // SetupWifi

void WifiCheckStatus()
{
#ifdef WIFI_AP_MODE
    return;
#else  // ! WIFI_AP_MODE

    wl_status_t wifiStatus = WiFi.status();

    static bool wasConnected = false;
    bool isConnected = wifiStatus == WL_CONNECTED;
    if (isConnected == wasConnected) return;
    wasConnected = isConnected;

    if (isConnected)
    {
        Serial.printf_P(PSTR("Connected to Wi-Fi SSID '%s'\n"), WIFI_SSID);
        Serial.printf_P(PSTR("Wi-Fi signal strength (RSSI): %ld dB\n"), WiFi.RSSI());

        // Might decrease number of packet CRC errors in case the board was previously using Wi-Fi (Wi-Fi settings are
        // persistent?)
        wifi_set_sleep_type(NONE_SLEEP_T);

        Serial.print(F("Please surf to: http://"));
        Serial.print(WiFi.localIP());
        Serial.println(F("/MFD.html"));
    }
    else
    {
        Serial.println(F("Wi-Fi DISconnected!"));
    } // if
#endif // WIFI_AP_MODE
} // WifiCheckStatus

const char* WifiDataToJson(const IPAddress& client, char* buf, const int n)
{
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"wifi_client_ip\": \"%u.%u.%u.%u\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter, client[0], client[1], client[2], client[3]);

    // JSON buffer overflow?
    if (at >= n) return "";

    #ifdef PRINT_JSON_BUFFERS_ON_SERIAL

    Serial.print(F("Wi-Fi data as JSON object:\n"));
    PrintJsonText(buf);

    #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return buf;
} // WifiDataToJson