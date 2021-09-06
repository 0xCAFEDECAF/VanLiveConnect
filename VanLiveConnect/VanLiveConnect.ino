/*
 * VanLiveConnect - Show all received information from the VAN bus on a "live" web page.
 *
 * Written by Erik Tromp
 *
 * Version 0.0.1 - June, 2021
 *
 * MIT license, all text above must be included in any redistribution.
 *
 * Documentation, details: see the 'README.md' file.
 */

#include "Config.h"

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <VanBusRx.h>

#ifdef USE_MDNS
#include <ESP8266mDNS.h>
#endif // USE_MDNS

// Either this for "WebSockets" (https://github.com/Links2004/arduinoWebSockets):
#include <WebSocketsServer.h>

// Or this for "WebSockets_Generic" (https://github.com/khoih-prog/WebSockets_Generic):
//#include <WebSocketsServer_Generic.h>

#ifdef WIFI_AP_MODE

#include <DNSServer.h>

#define DNS_PORT (53)
DNSServer dnsServer;
IPAddress apIP;

#endif // WIFI_AP_MODE

// Encourage the compiler to use the a newer (C++11 ?) standard. If this is removed, it doesn't compile!
char dummy_var_to_use_cppXX[] PROGMEM = R"==(")==";

// Over The Air (OTA) update, defined in BasicOTA.ino
void SetupOta();
void LoopOta();

// Persistent storage, defined in Store.ino
void SetupStore();

// Defined in WebServer.ino
void SetupWebServer();
void LoopWebServer();

// Create a web socket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Defined in Esp.ino
void PrintSystemSpecs();
const char* EspDataToJson(char* buf, const int n);

// Defined in Wifi.ino
void SetupWifi();
const char* WifiDataToJson(const IPAddress& client, char* buf, const int n);
const char* GetHostname();
void WifiCheckStatus();

enum VanPacketFilter_t
{
    VAN_PACKETS_ALL,
    VAN_PACKETS_NONE,
    VAN_PACKETS_HEAD_UNIT,
    VAN_PACKETS_AIRCON,
    VAN_PACKETS_COM2000_ETC,
    VAN_PACKETS_SAT_NAV
}; // enum VanPacketFilter_t

// Infrared receiver

// Results returned from the IR decoder
typedef struct
{
    unsigned long value;  // Decoded value
    PGM_P buttonStr;
    int bits;  // Number of bits in decoded value
    volatile unsigned int* rawbuf;  // Raw intervals in 50 usec ticks
    int rawlen;  // Number of records in rawbuf
    bool held;
} TIrPacket;

// Defined in IRrecv.ino
void IrSetup();
const char* ParseIrPacketToJson(const TIrPacket& pkt);
bool IrReceive(TIrPacket& irPacket);

void SetupVanReceiver()
{
    // Having the default VAN packet queue size of 15 (see VanBusRx.h) seems too little given the time that
    // is needed to send a JSON packet over the Wi-Fi; seeing quite some "VAN PACKET QUEUE OVERRUN!" lines.
    // Looks like it should be set to at least 100.
    #define VAN_PACKET_QUEUE_SIZE 100

    // Set to GPIO pin connected to VAN bus transceiver output
    #define RX_PIN D2

    if (VanBusRx.Setup(RX_PIN, VAN_PACKET_QUEUE_SIZE))
    {
        Serial.printf_P(PSTR("VanBusRx queue of size %d is set up\n"), VanBusRx.QueueSize());
    } // if
} // SetupVanReceiver

// Defined in PacketToJson.ino
const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt);
const char* EquipmentStatusDataToJson(char* buf, const int n);
const char* SatnavEquipmentDetection(char* buf, const int n);
void PrintJsonText(const char* jsonBuffer);

uint8_t websocketNum = 0xFF;

// Broadcast a (JSON) message to all websocket clients
void BroadcastJsonText(const char* json)
{
    if (strlen(json) <= 0) return;
    if (websocketNum == 0xFF) return;

    delay(1); // Give some time to system to process other things?

    unsigned long start = millis();

    webSocket.broadcastTXT(json);
    //webSocket.sendTXT(websocketNum, json); // Alternative

    // Print a message if the websocket broadcast took outrageously long (normally it takes around 1-2 msec).
    // If that takes really long (seconds or more), the VAN bus Rx queue will overrun (remember, ESP8266 is
    // a single-thread system).
    unsigned long duration = millis() - start;
    if (duration > 100)
    {
        Serial.printf_P(
            PSTR("Sending %zu JSON bytes via 'webSocket.broadcastTXT' took: %lu msec\n"),
            strlen(json),
            duration);

        Serial.print(F("JSON object:\n"));
        PrintJsonText(json);
    } // if
} // BroadcastJsonText

void WebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length)
{
    switch(type)
    {
        case WStype_DISCONNECTED:
        {
            Serial.printf("Websocket [%u] Disconnected!\n", num);
            websocketNum = 0xFF;
        }
        break;

        case WStype_CONNECTED:
        {
            IPAddress clientIp = webSocket.remoteIP(num);
            Serial.printf("Websocket [%u] Connected from %d.%d.%d.%d url: %s\n",
                num,
                clientIp[0], clientIp[1], clientIp[2], clientIp[3],
                payload);

            websocketNum = num;

            #define STATUS_JSON_BUFFER_SIZE 1024
            char jsonBuffer[STATUS_JSON_BUFFER_SIZE];

            // Dump ESP system data to client
            BroadcastJsonText(EspDataToJson(jsonBuffer, STATUS_JSON_BUFFER_SIZE));

            // Dump Wi-Fi and IP data to client
            BroadcastJsonText(WifiDataToJson(clientIp, jsonBuffer, STATUS_JSON_BUFFER_SIZE));

            // Dump equipment status data, e.g. presence of sat nav and other devices
            BroadcastJsonText(EquipmentStatusDataToJson(jsonBuffer, STATUS_JSON_BUFFER_SIZE));
        }
        break;
    } // switch
} // WebSocketEvent

// For prototyping on Sonoff board
#if defined ARDUINO_ESP8266_GENERIC || defined ARDUINO_ESP8266_ESP01
#define LED_BUILTIN 13
#endif

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    delay(1000);
    Serial.begin(115200);
    Serial.println(F("Starting VAN bus \"Live Connect\" server"));

    PrintSystemSpecs();

    Serial.println(F("Initializing EEPROM"));
    EEPROM.begin(64);

    // Setup "Over The Air" (OTA) update
    SetupOta();

#ifdef WIFI_AP_MODE
    apIP.fromString(IP_ADDR);
#endif // WIFI_AP_MODE

    SetupWifi();

#ifdef WIFI_AP_MODE
    // If DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);
#endif // WIFI_AP_MODE

#ifdef USE_MDNS
    // Start the mDNS responder
    if (MDNS.begin(GetHostname())) Serial.println("mDNS responder started");
    else Serial.println("Error setting up MDNS responder!");
#endif // USE_MDNS

    SetupWebServer();

    webSocket.begin();
    webSocket.onEvent(WebSocketEvent);

#ifdef WIFI_AP_MODE
    Serial.print(F("Please surf to: http://"));
    Serial.print(apIP);
    Serial.println(F("/MFD.html"));
#endif // WIFI_AP_MODE

    SetupVanReceiver();

    IrSetup();
} // setup

void loop()
{
#ifdef WIFI_AP_MODE
    dnsServer.processNextRequest();
#endif // WIFI_AP_MODE

    WifiCheckStatus();

    LoopOta();

    webSocket.loop();
    LoopWebServer();

    // IR receiver
    TIrPacket irPacket;
    if (IrReceive(irPacket)) BroadcastJsonText(ParseIrPacketToJson(irPacket));

    // VAN bus receiver
    TVanPacketRxDesc pkt;
    bool isQueueOverrun = false;
    if (VanBusRx.Receive(pkt, &isQueueOverrun)) BroadcastJsonText(ParseVanPacketToJson(pkt));
    if (isQueueOverrun) Serial.print(F("VAN PACKET QUEUE OVERRUN!\n"));

    // Print statistics every 5 seconds
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 5000UL)  // Arithmetic has safe roll-over
    {
        lastUpdate = millis();
        VanBusRx.DumpStats(Serial);
    } // if
} // loop
