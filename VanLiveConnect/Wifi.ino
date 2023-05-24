
#include <ESP8266WiFi.h>

#include "Config.h"

// Defined in WebSocket.ino
extern WebSocketsServer webSocket;
extern uint8_t prevWebsocketNum;
extern volatile uint8_t websocketNum;

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

void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt)
{
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    Serial.printf_P(PSTR("%sWi-Fi client connected: "), TimeStamp());
    Serial.print(macToString(evt.mac));
    Serial.print(F("\n"));

    websocketNum = WEBSOCKET_INVALID_NUM;
    prevWebsocketNum = WEBSOCKET_INVALID_NUM;

    //webSocket.disconnect();
} // onStationConnected

void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt)
{
    Serial.printf_P(PSTR("%sWi-Fi client disconnected: "), TimeStamp());
    Serial.print(macToString(evt.mac));
    Serial.print(F("\n"));

    websocketNum = WEBSOCKET_INVALID_NUM;
    prevWebsocketNum = WEBSOCKET_INVALID_NUM;

    //webSocket.disconnect();
} // onStationDisconnected

void onProbeRequestPrint(const WiFiEventSoftAPModeProbeRequestReceived& evt)
{
    Serial.printf_P(PSTR("%sProbe request from: "), TimeStamp());
    Serial.print(macToString(evt.mac));
    Serial.print(" RSSI: ");
    Serial.print(evt.rssi);
    Serial.print(F("\n"));
} // onProbeRequestPrint

void onProbeRequestBlink(const WiFiEventSoftAPModeProbeRequestReceived&)
{
    // Flash the LED
    digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == LOW ? HIGH : LOW);
} // onProbeRequestBlink

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

WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;
WiFiEventHandler probeRequestHandler;

const char* SetupWifi()
{
    WiFi.hostname(GetHostname());

  #ifdef WIFI_AP_MODE

    static const char* wifiSsid = WIFI_SSID;

  #ifdef ON_DESK_MFD_ESP_MAC
    // The test setup on the desk has a slightly different SSID
    if (WiFi.macAddress() == ON_DESK_MFD_ESP_MAC) wifiSsid = WIFI_SSID" test";
  #endif // ON_DESK_MFD_ESP_MAC

    Serial.printf_P(PSTR("Setting up captive portal on Wi-Fi access point '%s'\n"), wifiSsid);

    WiFi.softAPdisconnect (true);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  #ifdef WIFI_PASSWORD
    WiFi.softAP(wifiSsid, WIFI_PASSWORD, WIFI_CHANNEL, WIFI_SSID_HIDDEN, 2);
  #else
    WiFi.softAP(wifiSsid, NULL, WIFI_CHANNEL, WIFI_SSID_HIDDEN, 2);
  #endif

    // Register event handlers
    stationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected);
    stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onStationDisconnected);
    //probeRequestHandler = WiFi.onSoftAPModeProbeRequestReceived(&onProbeRequestPrint);
    //probeRequestHandler = WiFi.onSoftAPModeProbeRequestReceived(&onProbeRequestBlink);

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
    //WiFi.setOutputPower(20.5);  // TODO - default value is any different? Better?

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

    // Flash the LED as long as Wi-Fi is not connected
    if (! isConnected) digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == LOW ? HIGH : LOW);  // Toggle the LED

    if (isConnected == wasConnected) return;
    wasConnected = isConnected;

    if (isConnected)
    {
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

        Serial.printf_P(PSTR("Connected to Wi-Fi SSID '%s'\n"), WIFI_SSID);
        Serial.printf_P(PSTR("Wi-Fi signal strength (RSSI): %ld dB\n"), WiFi.RSSI());

        // Might decrease number of packet CRC errors in case the board was previously using Wi-Fi (Wi-Fi settings are
        // persistent?)
        wifi_set_sleep_type(NONE_SLEEP_T);

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
    Serial.printf_P(PSTR("%sWi-Fi data as JSON object:\n"), TimeStamp());
    PrintJsonText(buf);
  #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return buf;
} // WifiDataToJson