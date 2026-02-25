/*
 * VanLiveConnect - Show all received information from the VAN bus on a "live" web page.
 *
 * Written by Erik Tromp
 *
 * Version 1.2.0 - September, 2025
 *
 * MIT license, all text above must be included in any redistribution.
 *
 * Documentation, details: see the 'README.md' file.
 */

#include <Arduino.h>

#include "Config.h"
#include "VanIden.h"
#include "VanLiveConnectVersion.h"

// We need access to class AsyncWebSocketClient private members _runQueue() and _messageQueue
#define private public
#ifdef USE_OLD_ESP_ASYNC_WEB_SERVER  // See line 220 of Config.h
  #include <ESPAsyncWebSrv.h>  // https://github.com/dvarrel/ESPAsyncWebSrv
#else
  #include <ESPAsyncWebServer.h>  // https://github.com/ESP32Async/ESPAsyncWebServer
#endif
#undef private

#include <EEPROM.h>
#include <VanBusRx.h>  // https://github.com/0xCAFEDECAF/VanBus

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

#ifdef ARDUINO_ARCH_ESP32
 #if ! defined ESP_ARDUINO_VERSION || ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  #define Network WiFi
 #endif
  #define system_get_free_heap_size esp_get_free_heap_size
  #define LED_ON HIGH
  #define LED_OFF LOW
#else
  #define Network WiFi
  #define LED_ON LOW
  #define LED_OFF HIGH
#endif // ARDUINO_ARCH_ESP32

// Defined in Sleep.ino
extern unsigned long lastActivityAt;

// Over The Air (OTA) update, defined in BasicOTA.ino
void SetupOta();
void LoopOta();

// Persistent storage, defined in Store.ino
void SetupStore();

// Defined in WebServer.ino
void SetupWebServer();
void LoopWebServer();

// Defined in WebSocket.ino
bool SendJsonOnWebSocket(const char* json, bool saveForLater = false, bool isTestMessage = false);
void SetupWebSocket();
void LoopWebSocket();

// Defined in Esp.ino
void PrintSystemSpecs();
const char* EspRuntimeDataToJson(char* buf, const int n);

// Defined in Wifi.ino
const char* SetupWifi();
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

String md5Checksum = "";

// The following VAN bus packets are considered very important, and should not be skipped when the VAN bus RX queue
// is overrunning
bool IRAM_ATTR IsVeryImportantPacket(const TVanPacketRxDesc& pkt)
{
    return
        pkt.DataLen() >= 3 &&
        (
            (pkt.Iden() == DEVICE_REPORT && pkt.Data()[0] == 0x07) // mfd_to_satnav_...
            || (pkt.Iden() == DEVICE_REPORT && pkt.Data()[0] == 0x8A)  // head_unit_report, head_unit_button_pressed
            || pkt.Iden() == CAR_STATUS1_IDEN  // Right-hand stalk button press
            || pkt.Iden() == CAR_STATUS2_IDEN  // Info and alarm popups
            || pkt.Iden() == MFD_LANGUAGE_UNITS_IDEN
            || pkt.Iden() == AUDIO_SETTINGS_IDEN
            || pkt.Iden() == SATNAV_STATUS_1_IDEN
            || pkt.Iden() == SATNAV_STATUS_2_IDEN
            || pkt.Iden() == SATNAV_GUIDANCE_IDEN
            || pkt.Iden() == SATNAV_REPORT_IDEN
            || pkt.Iden() == MFD_TO_SATNAV_IDEN
            || pkt.Iden() == SATNAV_TO_MFD_IDEN
        );
} // IsVeryImportantPacket

// For VAN-bus packets identified as "important", the JSON data will be kept for later sending if the WebSocket
// send queue is full. By default, the WebSocket has 8 slots; see AsyncWebSocket.h: "#define WS_MAX_QUEUED_MESSAGES 8".
bool IsImportantPacket(const TVanPacketRxDesc& pkt)
{
    return
        IsVeryImportantPacket(pkt)
        ||
        (
            pkt.DataLen() >= 3 &&
            (
                pkt.Iden() == ENGINE_IDEN
                || pkt.Iden() == LIGHTS_STATUS_IDEN
                || pkt.Iden() == HEAD_UNIT_IDEN
                || pkt.Iden() == AIRCON1_IDEN
            )
        );
} // IsImportantPacket

void SetupVanReceiver()
{
  #if defined VAN_RX_IFS_DEBUGGING
    #define VAN_PACKET_QUEUE_SIZE 25
  #else
    // Having the default VAN packet queue size of 15 (see VanBusRx.h) seems too little given the time that
    // is needed to send a JSON packet over the Wi-Fi; seeing quite some "VAN PACKET QUEUE OVERRUN!" lines.
    // Looks like it should be set to at least 30.
    #define VAN_PACKET_QUEUE_SIZE 30
  #endif

  #if VAN_BUS_VERSION_INT >= 000003003
    // When queue fills above 80%, start dropping non-essential packets
    VanBusRx.SetDropPolicy(VAN_PACKET_QUEUE_SIZE * 8 / 10, &IsVeryImportantPacket);
  #endif

  // Use #defines, not const int, so that the Serial.printf_P below shows the correct pin name on the console.
  #ifdef ARDUINO_ARCH_ESP32
   #ifdef CONFIG_IDF_TARGET_ESP32S2
    #define TX_PIN GPIO_NUM_18 // GPIO pin connected to VAN bus transceiver input
    #define RX_PIN GPIO_NUM_33 // GPIO pin connected to VAN bus transceiver output
   #else
    #define TX_PIN GPIO_NUM_16 // GPIO pin connected to VAN bus transceiver input
    #define RX_PIN GPIO_NUM_21 // GPIO pin connected to VAN bus transceiver output
   #endif
  #else // ! ARDUINO_ARCH_ESP32
    #define TX_PIN D3 // GPIO pin connected to VAN bus transceiver input
    #define RX_PIN D2 // GPIO pin connected to VAN bus transceiver output
  #endif // ARDUINO_ARCH_ESP32

    Serial.printf_P(PSTR("Setting up VAN bus receiver on pin %s (GPIO%u)\n"), XSTR(RX_PIN), RX_PIN);

    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, VAN_BIT_RECESSIVE);  // Set bus state to 'recessive' (CANH and CANL: not driven)

    if (VanBusRx.Setup(RX_PIN, VAN_PACKET_QUEUE_SIZE))
    {
      #ifdef VAN_BUS_VERSION
        Serial.printf_P(PSTR("VanBus version %s initialized; VanBusRx queue of size %d is set up\n"),
            PSTR(VAN_BUS_VERSION),
            VanBusRx.QueueSize()
        );
      #else
        Serial.printf_P(PSTR("VanBus initialized; VanBusRx queue of size %d is set up\n"),
            VanBusRx.QueueSize()
        );
      #endif // VAN_BUS_VERSION
    } // if


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

#include <PrintEx.h>  // https://github.com/Chris--A/PrintEx plus patches from https://github.com/0xCAFEDECAF/VanLiveConnect/tree/main/VanLiveConnect/Patches/Arduino/libraries/PrintEx

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
    if (p != nullptr) *p = '\0';

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

void PrintDebugDefines()
{
    Serial.print(F("Compiled with following debug #define's (see file 'Config.h'):\n"));
  #if defined DEBUG_IR_RECV || defined DEBUG_WEBSERVER || defined DEBUG_WEBSOCKET \
    || defined DEBUG_ORIGINAL_MFD || defined WIFI_STRESS_TEST \
    || defined SHOW_VAN_RX_STATS || defined PRINT_RAW_PACKET_DATA \
    || defined PRINT_JSON_BUFFERS_ON_SERIAL || defined PRINT_VAN_CRC_ERROR_PACKETS_ON_SERIAL

  #ifdef DEBUG_IR_RECV
    Serial.print(F("- DEBUG_IR_RECV\n"));
  #endif // DEBUG_IR_RECV
  #ifdef DEBUG_WEBSERVER
    Serial.print(F("- DEBUG_WEBSERVER\n"));
  #endif // DEBUG_WEBSERVER
  #ifdef DEBUG_WEBSOCKET
    Serial.printf_P(PSTR("- DEBUG_WEBSOCKET = %d\n"), DEBUG_WEBSOCKET);
  #endif // DEBUG_WEBSOCKET
  #ifdef DEBUG_ORIGINAL_MFD
    Serial.print(F("- DEBUG_ORIGINAL_MFD\n"));
  #endif // DEBUG_ORIGINAL_MFD
  #ifdef WIFI_STRESS_TEST
    Serial.printf_P(PSTR("- WIFI_STRESS_TEST = %d\n"), WIFI_STRESS_TEST);
  #endif // WIFI_STRESS_TEST
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
long sleepAfter = SLEEP_MS_AFTER_NO_VAN_BUS_ACTIVITY;

#if defined ARDUINO_ARCH_ESP32
 #if defined ESP_ARDUINO_VERSION && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
  // This sets Arduino Stack Size - comment this line to use default 8K stack size
  SET_LOOP_TASK_STACK_SIZE(16 * 1024);  // 16KB
 #endif
#endif

void setup()
{
    // Experiments show that Wi-Fi connectivity degrades when the power supply is unstable.
    // Therefore, try to use as little power as possible: keep built-in LED off.
  #ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LED_OFF);
  #endif

    WiFi.mode(WIFI_OFF);
  #ifndef ARDUINO_ARCH_ESP32
    WiFi.forceSleepBegin();
  #endif
    delay(1);

    md5Checksum = ESP.getSketchMD5();

    SetupSleep();

    // Does this prevent the following crash?
    // "Fatal exception:29 flag:2 (EXCEPTION) epc1:0x4000e1c3 epc2:0x00000000 epc3:0x00000000 excvaddr:0x00000018 depc:0x00000000"
    // (see also: https://github.com/espressif/esp-azure/issues/17 )
    Serial.setDebugOutput(false);

  #if defined ARDUINO_ARCH_ESP32 && defined CONFIG_IDF_TARGET_ESP32S2
    // Give COM port device driver time to start
    delay(3000);
  #endif

    Serial.begin(115200);
    Serial.printf_P(PSTR("\nStarting VAN bus \"Live Connect\" server version %s\n"), VAN_LIVE_CONNECT_VERSION);

    PrintDebugDefines();

  #ifdef WIFI_AP_MODE
    apIP.fromString(IP_ADDR);

    const char* wifiSsid =
  #endif
    SetupWifi();
    PrintSystemSpecs();

    Serial.print(F("Initializing EEPROM\n"));
    EEPROM.begin(64);

  #if defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS
    // Setup non-volatile storage on SPIFFS resp. LittleFS
    SetupStore();
  #endif // defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

    // Setup "Over The Air" (OTA) update
    SetupOta();

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

  #ifdef ON_DESK_MFD_ESP_MAC
    // On the desk test setup, go to sleep much quicker
    if (Network.macAddress() == ON_DESK_MFD_ESP_MAC) sleepAfter = 10000;
  #endif // ON_DESK_MFD_ESP_MAC

  #ifdef WIFI_STRESS_TEST_MFD_ESP_MAC
    // Wi-Fi stress test: don't go to sleep
    if (Network.macAddress() == WIFI_STRESS_TEST_MFD_ESP_MAC) sleepAfter = -1;
  #endif // WIFI_STRESS_TEST_MFD_ESP_MAC

    if (sleepAfter > 0)
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

    WifiCheckStatus();

    LoopOta();

    LoopWebSocket();
    LoopWebServer();

    // IR receiver
    TIrPacket irPacket;
    if (IrReceive(irPacket)) SendJsonOnWebSocket(ParseIrPacketToJson(irPacket), true);

    if (sleepAfter > 0

      #ifdef TEST_SETUP_KEEP_AWAKE
        // Prevent the ESP board from going to sleep as long as no VAN bus packets were received,
        // as often happens in the test setup. In a "real" setup, this might drain the battery
        // when the board wakes up by accident (although I have never seen that happen).
        && VanBusRx.GetCount() > 0
      #endif
       )
    {
        if (millis() - lastActivityAt >= (unsigned long)sleepAfter)
        {
            GoToSleep();
            return;
        } // if
    } // if

    // VAN bus receiver

    TVanPacketRxDesc pkt;
    bool isQueueOverrun = false;
    if (VanBusRx.Receive(pkt, &isQueueOverrun))
    {
        lastActivityAt = millis();

      #if VAN_BUS_VERSION_INT >= 000003001 && VAN_BUS_VERSION_INT < 000003003

        // If RX queue is starting to overrun, keep only important (sat nav, stalk button press) packets
        #define PANIC_AT_PERCENTAGE (60)

        int nDiscarded = 0;
        while (VanBusRx.GetNQueued() * 100 / VanBusRx.QueueSize() > PANIC_AT_PERCENTAGE && ! IsVeryImportantPacket(pkt))
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

      #ifdef VAN_RX_IFS_DEBUGGING
        if (pkt.getIfsDebugPacket().IsAbnormal()) pkt.getIfsDebugPacket().Dump(Serial);
      #endif // VAN_RX_IFS_DEBUGGING

        SendJsonOnWebSocket(ParseVanPacketToJson(pkt), IsImportantPacket(pkt));
    }

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

        // Send ESP runtime data to client
        SendJsonOnWebSocket(EspRuntimeDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

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

    delay(9);
} // loop
