/*
 * VanLiveConnect - Show all received information from the VAN bus on a "live" web page.
 *
 * Written by Erik Tromp
 *
 * Version 1.0.0 - March, 2023
 *
 * MIT license, all text above must be included in any redistribution.
 *
 * Documentation, details: see the 'README.md' file.
 */

#include "Config.h"
#include "VanIden.h"

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

#define XSTR(x) STR(x)
#define STR(x) #x

// Over The Air (OTA) update, defined in BasicOTA.ino
void SetupOta();
void LoopOta();

// Persistent storage, defined in Store.ino
void SetupStore();

// Defined in WebServer.ino
void SetupWebServer();
void LoopWebServer();

// Defined in WebSocket.ino
void SendJsonOnWebSocket(const char* json, bool savePacketForLater = false);
void SetupWebSocket();
void LoopWebSocket();

// Defined in Esp.ino
void PrintSystemSpecs();
const char* EspRuntimeDataToJson(char* buf, const int n);

// Defined in Wifi.ino
const char* SetupWifi();
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

#include <WebSockets.h>  // For #define WEBSOCKETS_NETWORK_TYPE, used below

// The following VAN bus packets are considered important, and should not be skipped when the RX queue
// is overrunning
bool IsImportantPacket(const TVanPacketRxDesc& pkt)
{
    return
        pkt.DataLen() >= 3 &&
        (
            (pkt.Iden() == DEVICE_REPORT && pkt.Data()[0] == 0x07) // mfd_to_satnav_...
            || pkt.Iden() == SATNAV_STATUS_1_IDEN
            || pkt.Iden() == SATNAV_GUIDANCE_IDEN
            || pkt.Iden() == SATNAV_REPORT_IDEN
            || pkt.Iden() == SATNAV_TO_MFD_IDEN

            || pkt.Iden() == CAR_STATUS1_IDEN  // Right-hand stalk button press

            || (pkt.Iden() == DEVICE_REPORT && pkt.Data()[0] == 0x8A)  // head_unit_report, head_unit_button_pressed
        );
} // IsImportantPacket

void SetupVanReceiver()
{
    // Having the default VAN packet queue size of 15 (see VanBusRx.h) seems too little given the time that
    // is needed to send a JSON packet over the Wi-Fi; seeing quite some "VAN PACKET QUEUE OVERRUN!" lines.
    // Looks like it should be set to at least 100.
  #if defined VAN_RX_IFS_DEBUGGING
    #define VAN_PACKET_QUEUE_SIZE 50
  #elif defined VAN_RX_ISR_DEBUGGING
    #define VAN_PACKET_QUEUE_SIZE 60
  #else
    #if WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC
      // Async TCP mode causes a lot less hiccups, so we can do with a much smaller RX queue
      #define VAN_PACKET_QUEUE_SIZE 60
    #else
      // Sync TCP mode can cause hiccups of several seconds, even a value of 150 is not enough... But more
      // than this uses too much RAM :-(
      #define VAN_PACKET_QUEUE_SIZE 150
    #endif
  #endif

    const int TX_PIN = D3; // GPIO pin connected to VAN bus transceiver input
    const int RX_PIN = D2; // GPIO pin connected to VAN bus transceiver output

    Serial.printf_P(PSTR("Setting up VAN bus receiver on pin %s (GPIO%u)\n"), XSTR(RX_PIN), RX_PIN);

    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, VAN_BIT_RECESSIVE);  // Set bus state to 'recessive' (CANH and CANL: not driven)

    if (VanBusRx.Setup(RX_PIN, VAN_PACKET_QUEUE_SIZE))
    {
      #ifdef VAN_BUS_VERSION
        Serial.printf_P(PSTR("VanBus version %S initialized; VanBusRx queue of size %d is set up\n"),
            PSTR(VAN_BUS_VERSION),
            VanBusRx.QueueSize()
        );
      #else
        Serial.printf_P(PSTR("VanBus initialized; VanBusRx queue of size %d is set up\n"),
            VanBusRx.QueueSize()
        );
      #endif // VAN_BUS_VERSION
    } // if

  #if VAN_BUS_VERSION_INT >= 000003003
    // When queue fills above 80%, start dropping non-essential packets
    VanBusRx.SetDropPolicy(VAN_PACKET_QUEUE_SIZE * 8 / 10, &IsImportantPacket);
  #endif

  #if defined VAN_RX_ISR_DEBUGGING || defined VAN_RX_IFS_DEBUGGING
    Serial.printf_P(PSTR("==> VanBusRx: DEBUGGING MODE IS ON!\n"));
  #endif
} // SetupVanReceiver

// Defined in PacketToJson.ino
const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt);

// Defined in Sleep.ino
void SetupSleep();
void GoToSleep();

// Defined in DateTime.ino
const char* DateTime(time_t, boolean = false);
void PrintTimeStamp();

// TODO - reduce size of large JSON packets like the ones containing guidance instruction icons
#define JSON_BUFFER_SIZE 4096
char jsonBuffer[JSON_BUFFER_SIZE];

#ifdef SHOW_VAN_RX_STATS

#include <PrintEx.h>

const char* VanBusStatsToStr()
{
    static const int bufSize = 120;
    static char buffer[bufSize];

    GString str(buffer);
    PrintAdapter streamer(str);

    VanBusRx.DumpStats(streamer, false);

    // Replace '\n' by string terminator '\0'
    buffer[bufSize - 1] = '\0';
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
            "\"van_bus_stats\": \"VAN Rx Stats: %s\",\n"
            "\"uptime_seconds\": \"%lu\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter, VanBusStatsToStr(), millis() / 1000);

    // JSON buffer overflow?
    if (at >= n) return "";

    return buf;
} // VanBusStatsToJson

#endif // SHOW_VAN_RX_STATS

void PrintDebugDefines()
{
    Serial.print(F("Compiled with following debug #define's (see file 'Config.h'):\n"));
  #if defined DEBUG_IR_RECV || defined DEBUG_WEBSERVER || defined DEBUG_WEBSOCKET \
    || defined DEBUG_ORIGINAL_MFD || defined SHOW_VAN_RX_STATS || defined PRINT_RAW_PACKET_DATA \
    || defined PRINT_JSON_BUFFERS_ON_SERIAL || defined PRINT_VAN_CRC_ERROR_PACKETS_ON_SERIAL

  #ifdef DEBUG_IR_RECV
    Serial.print(F("- DEBUG_IR_RECV\n"));
  #endif // DEBUG_IR_RECV
  #ifdef DEBUG_WEBSERVER
    Serial.print(F("- DEBUG_WEBSERVER\n"));
  #endif // DEBUG_WEBSERVER
  #ifdef DEBUG_WEBSOCKET
    Serial.print(F("- DEBUG_WEBSOCKET\n"));
  #endif // DEBUG_WEBSOCKET
  #ifdef DEBUG_ORIGINAL_MFD
    Serial.print(F("- DEBUG_ORIGINAL_MFD\n"));
  #endif // DEBUG_ORIGINAL_MFD
  #ifdef SHOW_VAN_RX_STATS
    Serial.print(F("- SHOW_VAN_RX_STATS\n"));
  #endif // SHOW_VAN_RX_STATS
  #ifdef PRINT_RAW_PACKET_DATA
    Serial.print(F("- PRINT_RAW_PACKET_DATA\n"));
  #endif // PRINT_RAW_PACKET_DATA
  #ifdef PRINT_JSON_BUFFERS_ON_SERIAL
    Serial.print(F("- PRINT_JSON_BUFFERS_ON_SERIAL\n"));
  #endif // PRINT_JSON_BUFFERS_ON_SERIAL
  #ifdef PRINT_VAN_CRC_ERROR_PACKETS_ON_SERIAL
    Serial.print(F("- PRINT_VAN_CRC_ERROR_PACKETS_ON_SERIAL\n"));
  #endif // PRINT_VAN_CRC_ERROR_PACKETS_ON_SERIAL

  #else
    Serial.print(F("<none>\n"));
  #endif // defined ...
} // PrintDebugDefines

// After a few minutes of VAN bus inactivity, go to sleep to save power
unsigned long sleepAfter = SLEEP_MS_AFTER_NO_VAN_BUS_ACTIVITY;

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    SetupSleep();

    // Does this prevent the following crash?
    // "Fatal exception:29 flag:2 (EXCEPTION) epc1:0x4000e1c3 epc2:0x00000000 epc3:0x00000000 excvaddr:0x00000018 depc:0x00000000"
    // (see also: https://github.com/espressif/esp-azure/issues/17 )
    Serial.setDebugOutput(false);

    delay(1000);
    Serial.begin(115200);
    Serial.print(F("\nStarting VAN bus \"Live Connect\" server\n"));

    PrintDebugDefines();
    PrintSystemSpecs();

    Serial.print(F("Initializing EEPROM\n"));
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

    const char* wifiSsid = SetupWifi();

  #ifdef WIFI_AP_MODE
    // If DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);
  #endif // WIFI_AP_MODE

  #ifdef USE_MDNS
    // Start the mDNS responder
    if (MDNS.begin(GetHostname())) Serial.print("mDNS responder started\n");
    else Serial.print("Error setting up MDNS responder!\n");
  #endif // USE_MDNS

    SetupWebServer();
    SetupWebSocket();

  #ifdef WIFI_AP_MODE
    Serial.printf_P(PSTR("Please connect to Wi-Fi network '%s', then surf to: http://"), wifiSsid);
    Serial.print(apIP);
    Serial.print(F("/MFD.html\n"));
  #endif // WIFI_AP_MODE

    SetupVanReceiver();

    IrSetup();

    sleepAfter = SLEEP_MS_AFTER_NO_VAN_BUS_ACTIVITY;
  #if 0
  #ifdef ON_DESK_MFD_ESP_MAC
    // On the desk test setup, we want to go to sleep much quicker
    if (WiFi.macAddress() == ON_DESK_MFD_ESP_MAC) sleepAfter = 10000;
  #endif // ON_DESK_MFD_ESP_MAC
  #endif // 0

    if (sleepAfter != -1)
    {
        Serial.printf_P
        (
            PSTR
            (
                "ESP will enter light sleep mode after %lu:%02lu minutes of inactivity; "
                "listening on pin %s (GPIO%u) for VAN bus activity to wake up from sleep.\n"
            ),
            sleepAfter / 1000 / 60, sleepAfter / 1000 % 60,
            XSTR(LIGHT_SLEEP_WAKE_PIN),
            LIGHT_SLEEP_WAKE_PIN
        );
    } // if
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
    if (IrReceive(irPacket)) SendJsonOnWebSocket(ParseIrPacketToJson(irPacket), true);

    LoopWebSocket(); // TODO - necessary?

    static unsigned long lastPacketAt = 0;
    if (sleepAfter != -1)
    {
        if (millis() - lastPacketAt >= sleepAfter)
        {
            GoToSleep();
            lastPacketAt = millis();
            return;
        } // if
    } // if

    // VAN bus receiver

    TVanPacketRxDesc pkt;
    bool isQueueOverrun = false;
    if (VanBusRx.Receive(pkt, &isQueueOverrun))
    {
        lastPacketAt = millis();

      #if VAN_BUS_VERSION_INT >= 000003001 && VAN_BUS_VERSION_INT < 000003003

        // If RX queue is starting to overrun, keep only important (sat nav, stalk button press) packets
        #if WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC
          #define PANIC_AT_PERCENTAGE (60)
        #else
          // Sync TCP mode can cause hiccups of several seconds, so start throwing away packets very soon
          #define PANIC_AT_PERCENTAGE (40)
        #endif
        int nDiscarded = 0;
        while (VanBusRx.GetNQueued() * 100 / VanBusRx.QueueSize() > PANIC_AT_PERCENTAGE && ! IsImportantPacket(pkt))
        {
            bool isQueueOverrun2 = false;
            if (! VanBusRx.Receive(pkt, &isQueueOverrun2)) break;
            isQueueOverrun = isQueueOverrun || isQueueOverrun2;
            nDiscarded++;
        } // while

        if (nDiscarded > 0)
        {
            Serial.printf_P(PSTR("==> Discarded %u VAN bus packets to prevent RX queue overflow\n"), nDiscarded);
        } // if

      #endif

        LoopWebSocket(); // TODO - necessary?

      #ifdef VAN_RX_IFS_DEBUGGING
        if (pkt.getIfsDebugPacket().IsAbnormal()) pkt.getIfsDebugPacket().Dump(Serial);
      #endif // VAN_RX_IFS_DEBUGGING

        LoopWebSocket(); // TODO - necessary?

        SendJsonOnWebSocket(ParseVanPacketToJson(pkt), IsImportantPacket(pkt));
    }

    LoopWebSocket(); // TODO - necessary?

    if (isQueueOverrun)
    {
        Serial.print(F("VAN PACKET QUEUE OVERRUN!\n"));

      #ifdef SHOW_VAN_RX_STATS
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
      #endif // SHOW_VAN_RX_STATS
    } // if

    LoopWebSocket(); // TODO - necessary?

    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 5000UL)  // Arithmetic has safe roll-over
    {
        lastUpdate = millis();

      #if VAN_BUS_VERSION_INT >= 000003002
        if (! VanBusRx.IsEnabled())
        {
            Serial.print(F("==> Random noise detected on VAN bus; re-starting receiver\n"));
            VanBusRx.Enable();
        } // if
      #endif

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
        PrintTimeStamp();
        VanBusRx.DumpStats(Serial);
    } // if
  #endif // SHOW_VAN_RX_STATS
} // loop
