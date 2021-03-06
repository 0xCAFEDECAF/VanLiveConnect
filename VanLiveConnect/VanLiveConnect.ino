/*
 * VanLiveConnect - Show all received information from the VAN bus on a "live" web page.
 *
 * Written by Erik Tromp
 *
 * Version 0.0.1 - September, 2021
 *
 * MIT license, all text above must be included in any redistribution.
 *
 * Documentation, details: see the 'README.md' file.
 */

#include "Config.h"

#include <EEPROM.h>
#include <VanBusRx.h>

#ifdef USE_MDNS
#include <ESP8266mDNS.h>
#endif // USE_MDNS

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

// Defined in WebSocket.ino
void SendJsonOnWebSocket(const char* json);
void SetupWebSocket();
void LoopWebSocket();

// Defined in Esp.ino
void PrintSystemSpecs();
const char* EspRuntimeDataToJson(char* buf, const int n);

// Defined in Wifi.ino
void SetupWifi();
const char* WifiDataToJson(const IPAddress& client, char* buf, const int n);
const char* GetHostname();
void WifiCheckStatus();

enum VanPacketFilter_t
{
    VAN_PACKETS_ALL_VAN_PKTS,
    VAN_PACKETS_NO_VAN_PKTS,
    VAN_PACKETS_HEAD_UNIT_PKTS,
    VAN_PACKETS_AIRCON_PKTS,
    VAN_PACKETS_COM2000_ETC_PKTS,
    VAN_PACKETS_SAT_NAV_PKTS
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
    unsigned long millis_;
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
  #if defined VAN_RX_ISR_DEBUGGING || defined VAN_RX_IFS_DEBUGGING
    #define VAN_PACKET_QUEUE_SIZE 50
  #else
    #define VAN_PACKET_QUEUE_SIZE 250
  #endif // VAN_RX_ISR_DEBUGGING || VAN_RX_IFS_DEBUGGING

    // GPIO pin connected to VAN bus transceiver output
    #define RX_PIN D2

    if (VanBusRx.Setup(RX_PIN, VAN_PACKET_QUEUE_SIZE))
    {
        Serial.printf_P(PSTR("VanBusRx queue of size %d is set up\n"), VanBusRx.QueueSize());
    } // if

  #if defined VAN_RX_ISR_DEBUGGING || defined VAN_RX_IFS_DEBUGGING
    Serial.printf_P(PSTR("--> VanBusRx: DEBUGGING MODE IS ON!\n"));
  #endif
} // SetupVanReceiver

// Defined in PacketToJson.ino
const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt);
const char* EquipmentStatusDataToJson(char* buf, const int n);
const char* SatnavEquipmentDetection(char* buf, const int n);
void PrintJsonText(const char* jsonBuffer);

// TODO - reduce size of large JSON packets like the ones containing guidance instruction icons
#define JSON_BUFFER_SIZE 4096
char jsonBuffer[JSON_BUFFER_SIZE];

#ifdef SHOW_VAN_RX_STATS

#include <PrintEx.h>

const char* VanBusStatsToStr()
{
    #define BUFFER_SIZE 120
    static char buffer[BUFFER_SIZE];

    GString str(buffer);
    PrintAdapter streamer(str);

    VanBusRx.DumpStats(streamer, false);

    // Replace '\n' by string terminator '\0'
    buffer[BUFFER_SIZE - 1] = '\0';
    char *p = strchr(buffer, '\n');
    if (p != NULL) *p = '\0';

    return buffer;
} // VanBusStatsToStr

const char* VanBusStatsToJson(char* buf, const int n)
{
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"van_bus_stats\": \"VAN Rx Stats: %s\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter, VanBusStatsToStr());

    // JSON buffer overflow?
    if (at >= n) return "";

    return buf;
} // VanBusStatsToJson

#endif // SHOW_VAN_RX_STATS

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

  #ifdef SERVE_FROM_SPIFFS
    // Setup non-volatile storage on SPIFFS
    SetupStore();
  #endif // SERVE_FROM_SPIFFS

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
    SetupWebSocket();

  #ifdef WIFI_AP_MODE
    Serial.printf_P(PSTR("Please connect to Wi-Fi network '%s', then surf to: http://"), WIFI_SSID);
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

    LoopWebSocket(); // TODO - necessary?

    WifiCheckStatus();

    LoopWebSocket(); // TODO - necessary?

    LoopOta();

    LoopWebSocket(); // TODO - necessary?

    LoopWebSocket();
    LoopWebServer();

    LoopWebSocket(); // TODO - necessary?

    // IR receiver
    TIrPacket irPacket;
    if (IrReceive(irPacket)) SendJsonOnWebSocket(ParseIrPacketToJson(irPacket));

    LoopWebSocket(); // TODO - necessary?

    // VAN bus receiver
    TVanPacketRxDesc pkt;
    bool isQueueOverrun = false;
    if (VanBusRx.Receive(pkt, &isQueueOverrun))
    {
        LoopWebSocket(); // TODO - necessary?

      #ifdef VAN_RX_IFS_DEBUGGING
        if (pkt.getIfsDebugPacket().IsAbnormal()) pkt.getIfsDebugPacket().Dump(Serial);
      #endif // VAN_RX_IFS_DEBUGGING

        LoopWebSocket(); // TODO - necessary?

        SendJsonOnWebSocket(ParseVanPacketToJson(pkt));
    }

    LoopWebSocket(); // TODO - necessary?

    if (isQueueOverrun)
    {
        Serial.print(F("VAN PACKET QUEUE OVERRUN!\n"));

      #ifdef DEBUG_WEBSOCKET
        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"van_bus_overrun\": \"YES\"\n"
            "}\n"
        "}\n";

        snprintf_P(jsonBuffer, JSON_BUFFER_SIZE, jsonFormatter);
        SendJsonOnWebSocket(jsonBuffer);
      #endif // DEBUG_WEBSOCKET
    } // if

    LoopWebSocket(); // TODO - necessary?

    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 5000UL)  // Arithmetic has safe roll-over
    {
        lastUpdate = millis();

      #ifdef SHOW_ESP_RUNTIME_STATS
        // Send ESP runtime data to client
        SendJsonOnWebSocket(EspRuntimeDataToJson(jsonBuffer, JSON_BUFFER_SIZE));
      #endif // SHOW_ESP_RUNTIME_STATS

      #ifdef SHOW_VAN_RX_STATS
        // Send VAN bus receiver status string to client
        SendJsonOnWebSocket(VanBusStatsToJson(jsonBuffer, JSON_BUFFER_SIZE));
      #endif // SHOW_VAN_RX_STATS
    } // if

  #ifdef SHOW_VAN_RX_STATS
    static unsigned long lastUpdate2 = 0;
    if (millis() - lastUpdate2 >= 15000UL)  // Arithmetic has safe roll-over
    {
        lastUpdate2 = millis();

        // Print statistics
        VanBusRx.DumpStats(Serial);
    } // if
  #endif // SHOW_VAN_RX_STATS
} // loop
