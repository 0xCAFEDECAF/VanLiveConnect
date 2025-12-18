
#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <esp_wifi.h>
#else
  #include <ESP8266WiFi.h>
#endif // ARDUINO_ARCH_ESP32

#include "Config.h"

// Defined in WebSocket.ino
extern AsyncWebSocket webSocket;
extern uint32_t websocketId_1;
extern uint32_t websocketId_2;

const char* GetHostname()
{
    return HOST_NAME;
} // GetHostname

String macToString(const unsigned char* mac)
{
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
} // macToString

#ifdef ARDUINO_ARCH_ESP32
// WARNING: This function is called from a separate FreeRTOS task (thread)!
void onStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  #ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, LED_OFF);
  #endif

    Serial.printf_P(PSTR("%sWi-Fi client connected: "), TimeStamp());
  #if defined ESP_ARDUINO_VERSION && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
    Serial.print(macToString(info.wifi_ap_staconnected.mac));
  #else
    Serial.print(macToString(info.sta_connected.mac));
  #endif
    Serial.print("\n");
}

#else
void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt)
{
  #ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, LED_OFF);
  #endif

    Serial.printf_P(PSTR("%sWi-Fi client connected: "), TimeStamp());
    Serial.print(macToString(evt.mac));
    Serial.print("\n");
} // onStationConnected

#endif // ARDUINO_ARCH_ESP32

#ifdef ARDUINO_ARCH_ESP32
// WARNING: This function is called from a separate FreeRTOS task (thread)!
void onStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.printf_P(PSTR("%sWi-Fi client disconnected: "), TimeStamp());
  #if defined ESP_ARDUINO_VERSION && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
    Serial.print(macToString(info.wifi_ap_stadisconnected.mac));
  #else
    Serial.print(macToString(info.sta_disconnected.mac));
  #endif
    Serial.print("\n");
}

#else
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt)
{
    Serial.printf_P(PSTR("%sWi-Fi client disconnected: "), TimeStamp());
    Serial.print(macToString(evt.mac));
    Serial.print("\n");

  #if 0
    // Note: even though the webSocket is closed here, the resources will be freed only after a re-connection.
    // This is because the resources are associated to a TCP/IP connection, which can only be closed successfully
    // when there is a connection.
    if (websocketId_1 != WEBSOCKET_INVALID_ID) webSocket.close(websocketId_1);
    if (websocketId_2 != WEBSOCKET_INVALID_ID) webSocket.close(websocketId_2);

    websocketId_1 = WEBSOCKET_INVALID_ID;
    websocketId_2 = WEBSOCKET_INVALID_ID;
  #endif // 0
} // onStationDisconnected

#endif // ARDUINO_ARCH_ESP32

#if 0
void onProbeRequestPrint(const WiFiEventSoftAPModeProbeRequestReceived& evt)
{
    Serial.printf_P(PSTR("%sProbe request from: "), TimeStamp());
    Serial.print(macToString(evt.mac));
    Serial.print(F(" RSSI: "));
    Serial.print(evt.rssi);
    Serial.print("\n");
} // onProbeRequestPrint

void onProbeRequestBlink(const WiFiEventSoftAPModeProbeRequestReceived&)
{
    // Flash the LED
    digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == LOW ? HIGH : LOW);
} // onProbeRequestBlink
#endif // 0

#ifndef WIFI_AP_MODE // Wi-Fi access point mode
void WifiConfig()
{
  #ifndef USE_DHCP
    // Fixed IP configuration, e.g. when using Android / Windows ICS Wi-Fi hotspot
    IPAddress ip; ip.fromString(IP_ADDR);
    IPAddress gateway; gateway.fromString(IP_GATEWAY);
    IPAddress subnet; subnet.fromString(IP_SUBNET);
    WiFi.config(ip, gateway, subnet);
  #endif // ifndef USE_DHCP
} // WifiConfig
#endif // ifdef WIFI_AP_MODE

#ifndef ARDUINO_ARCH_ESP32
WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;
WiFiEventHandler probeRequestHandler;
#endif // ARDUINO_ARCH_ESP32

const char* const AUTH_MODE_NAMES[]{ "AUTH_OPEN", "AUTH_WEP", "AUTH_WPA_PSK", "AUTH_WPA2_PSK", "AUTH_WPA_WPA2_PSK", "AUTH_MAX" };

#ifdef ARDUINO_ARCH_ESP32
  #define softap_config wifi_ap_config_t
#endif // ARDUINO_ARCH_ESP32

void PrintSoftApConfig(softap_config const& config)
{
    Serial.print("\n");
    Serial.print(F("SoftAP Configuration\n"));
    Serial.print(F("--------------------\n"));

    Serial.print(F("ssid           : "));
    Serial.print((char *) config.ssid);
    Serial.print("\n");

    Serial.print(F("password       : "));
    Serial.print((char *) config.password);
    Serial.print("\n");

    Serial.print(F("ssid_len       : "));
    Serial.print(config.ssid_len);
    Serial.print("\n");

    Serial.print(F("channel        : "));
    Serial.print(config.channel);
    Serial.print("\n");

    Serial.print(F("authmode       : "));
    Serial.print(AUTH_MODE_NAMES[config.authmode]);
    Serial.print("\n");

    Serial.print(F("ssid_hidden    : "));
    Serial.print(config.ssid_hidden);
    Serial.print("\n");

    Serial.print(F("max_connection : "));
    Serial.print(config.max_connection);
    Serial.print("\n");

    Serial.print(F("beacon_interval: "));
    Serial.print(config.beacon_interval);
    Serial.print("ms\n");

    Serial.print(F("--------------------\n"));
    Serial.print("\n");
} // PrintSoftApConfig

const char* SetupWifi()
{
    WiFi.hostname(GetHostname());

    // Seems to help in decreasing the jitter on the VAN bus bit timings. But also seems to
    // deteriorate Wi-Fi connectivity.
    //wifi_set_sleep_type(NONE_SLEEP_T);

    static const char* wifiSsid = WIFI_SSID;

  #ifdef WIFI_AP_MODE

  #ifdef ON_DESK_MFD_ESP_MAC
    // The test setup on the desk has a slightly different SSID
    if (Network.macAddress() == ON_DESK_MFD_ESP_MAC) wifiSsid = WIFI_SSID" test";
  #endif // ON_DESK_MFD_ESP_MAC

    Serial.printf_P(PSTR("Setting up captive portal on Wi-Fi access point '%s', channel %d\n"), wifiSsid, WIFI_CHANNEL);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    // Register event handlers
  #ifdef ARDUINO_ARCH_ESP32
   #if defined ESP_ARDUINO_VERSION && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
    WiFi.onEvent(onStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(onStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
   #else
    WiFi.onEvent(onStationConnected, WiFiEvent_t::SYSTEM_EVENT_AP_STACONNECTED);
    WiFi.onEvent(onStationDisconnected, WiFiEvent_t::SYSTEM_EVENT_AP_STADISCONNECTED);
   #endif
  #else
    stationConnectedHandler = WiFi.onSoftAPModeStationConnected(onStationConnected);
    stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(onStationDisconnected);
    //probeRequestHandler = WiFi.onSoftAPModeProbeRequestReceived(onProbeRequestPrint);
    //probeRequestHandler = WiFi.onSoftAPModeProbeRequestReceived(onProbeRequestBlink);
  #endif // ARDUINO_ARCH_ESP32

  #ifdef WIFI_PASSWORD
    WiFi.softAP(wifiSsid, WIFI_PASSWORD, WIFI_CHANNEL, WIFI_SSID_HIDDEN);
  #else
    WiFi.softAP(wifiSsid, nullptr, WIFI_CHANNEL, WIFI_SSID_HIDDEN);
  #endif

    softap_config config_ap;
  #ifdef ARDUINO_ARCH_ESP32
    wifi_config_t config;
    esp_wifi_get_config(WIFI_IF_AP, &config);
    config_ap = config.ap;
  #else
    wifi_softap_get_config(&config_ap);
  #endif // ARDUINO_ARCH_ESP32
    PrintSoftApConfig(config_ap);

  #else  // ! WIFI_AP_MODE

    Serial.printf_P(PSTR("Connecting to Wi-Fi SSID '%s'\n"), WIFI_SSID);

    WifiConfig();

    WiFi.mode(WIFI_STA);  // Otherwise it may be in WIFI_AP_STA mode, broadcasting an SSID like AI_THINKER_XXXXXX
    WiFi.disconnect();  // After reset via HW button sometimes cannot seem to reconnect without this
    WiFi.persistent(false);
  #ifdef ARDUINO_ARCH_ESP32
   #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    WiFi.setAutoReconnect(true);
   #else // ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    WiFi.setAutoConnect(true);
   #endif // ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  #else // ! ARDUINO_ARCH_ESP32
    WiFi.setAutoConnect(true);
  #endif // ARDUINO_ARCH_ESP32

    // See https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html :
    //WiFi.setPhyMode(WIFI_PHY_MODE_11N);

  #ifdef WIFI_PASSWORD
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  #else
    WiFi.begin(WIFI_SSID);
  #endif

  #endif // WIFI_AP_MODE

    delay(1);

    return wifiSsid;
} // SetupWifi

void WifiCheckStatus()
{
  #ifdef WIFI_AP_MODE
    return;
  #else  // ! WIFI_AP_MODE

    static unsigned long lastUpdate;
    unsigned long now = millis();

    if (now - lastUpdate < 250) return;  // Arithmetic has safe roll-over
    lastUpdate = now;

    wl_status_t wifiStatus = WiFi.status();

    static bool wasConnected = false;
    bool isConnected = wifiStatus == WL_CONNECTED;

  #ifdef LED_BUILTIN
    // Flash the LED as long as Wi-Fi is not connected
    if (! isConnected) digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == LOW ? HIGH : LOW);  // Toggle the LED
  #endif

    if (isConnected == wasConnected) return;
    wasConnected = isConnected;

    if (isConnected)
    {
      #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LED_OFF);
      #endif

        Serial.printf_P(PSTR("Connected to Wi-Fi SSID '%s'\n"), WIFI_SSID);
        Serial.printf_P(PSTR("Wi-Fi signal strength (RSSI): %d dB\n"), WiFi.RSSI());

        // Seems to help in decreasing the jitter on the VAN bus bit timings. But also seems to
        // deteriorate Wi-Fi connectivity.
        //wifi_set_sleep_type(NONE_SLEEP_T);

        Serial.print(F("Please surf to: http://"));
        Serial.print(WiFi.localIP());
        Serial.print(F("/MFD.html\n"));
    }
    else
    {
        Serial.print(F("Wi-Fi DISconnected!\n"));
    } // if
#endif // WIFI_AP_MODE
} // WifiCheckStatus
