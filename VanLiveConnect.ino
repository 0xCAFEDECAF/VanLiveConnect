/*
 * VanLiveConnect - Show all received information from the VAN bus on a "live" web page.
 *
 * Written by Erik Tromp
 *
 * Version 0.0.1 - March, 2021
 *
 * MIT license, all text above must be included in any redistribution.
 *
 * -----
 * Documentation, details
 *
 * See the 'README.md' file.
 */

// Uncomment to see JSON buffers printed on the Serial port
#define PRINT_JSON_BUFFERS_ON_SERIAL

#include <ESP8266WiFi.h>

// Either this for "WebSockets" (https://github.com/Links2004/arduinoWebSockets):
#include <WebSocketsServer.h>

// Or this for "WebSockets_Generic" (https://github.com/khoih-prog/WebSockets_Generic):
//#include <WebSocketsServer_Generic.h>

#include <ESP8266WebServer.h>
#include <VanBusRx.h>

#include "Config.h"

int RX_PIN = D2; // Set to GPIO pin connected to VAN bus transceiver output

#include <DNSServer.h>

#ifdef WIFI_AP_MODE

#define DNS_PORT (53)
DNSServer dnsServer;
IPAddress apIP(AP_IP);

#endif // WIFI_AP_MODE

// Encourage the compiler to use the a newer (C++11 ?) standard. If this is removed, it doesn't compile!
char dummy_var_to_use_cpp11[] PROGMEM = R"==(")==";

// Over-the-air (OTA) update, defined in BasicOTA.ino
void setupOta();
void loopOta();

// Persistent storage, defined in Store.ino
void MarkStoreDirty();
void LoopStore();
struct StoredData
{
    bool satnavGuidanceActive;
    bool satnavDiscPresent;
    uint8_t satnavGuidancePreference;

    // TODO - increase to 100 (causes crash at boot time)
    #define MAX_DIRECTORY_ENTRIES 30
    String personalDirectoryEntries[MAX_DIRECTORY_ENTRIES];
    String professionalDirectoryEntries[MAX_DIRECTORY_ENTRIES];
};
StoredData* getStore();

ESP8266WebServer webServer;
WebSocketsServer webSocket = WebSocketsServer(81);

// Defined in Esp.ino
extern const String md5Checksum;
void PrintSystemSpecs();
const char* EspDataToJson();

// Returns true if the actual Etag is equal to the received Etag in an 'If-None-Match' header field.
// Shameless copy from: https://werner.rothschopf.net/microcontroller/202011_arduino_webserver_caching_en.htm .
bool checkETag(const String& etag)
{
    for (int i = 0; i < webServer.headers(); i++)
    {
        if (webServer.headerName(i).compareTo(F("If-None-Match")) == 0)
        {
            String read = webServer.header(i);
            read.replace("\"", ""); // some browsers (i.e. Samsung) discard the double quotes
            if (read == etag)
            {
                webServer.send(304, "text/plain", F("Not Modified"));
                Serial.println(
                    String(F("[webServer] ")) + webServer.headerName(i) + F(": ") + webServer.header(i) + F(" - Not Modified"));
                return true;
            } // if
        } // if
    } // for

    webServer.sendHeader(F("ETag"), String("\"") + etag + "\"");

    // Cache 10 seconds, then falls back to using the "If-None-Match" mechanism
    webServer.sendHeader(F("Cache-Control"), F("private, max-age=10"), true);

    return false;
} // checkETag

enum VanPacketFilter_t
{
    VAN_PACKETS_ALL_EXCEPT,
    VAN_PACKETS_NONE_EXCEPT,
    VAN_PACKETS_HEAD_UNIT,
    VAN_PACKETS_AIRCON,
    VAN_PACKETS_SAT_NAV
}; // enum VanPacketFilter_t

// serialDumpFilter == 0 means: no filtering; print all
// serialDumpFilter != 0 means: print only the packet + JSON data for the specified IDEN
uint16_t serialDumpFilter;

// Set a simple filter on the dumping of packet + JSON data on Serial.
// Surf to e.g. http://car.lan/dumpOnly?iden=8c4 to have only packets with IDEN 0x8C4 dumped on serial.
// Surf to http://car.lan/dumpOnly?iden=0 to dump all packets.
void handleDumpFilter()
{
    Serial.print(F("Web server received request from "));
    String ip = webServer.client().remoteIP().toString();
    Serial.print(ip);
    Serial.print(webServer.method() == HTTP_GET ? F(": GET - '") : F(": POST - '"));
    Serial.print(webServer.uri());
    bool found = false;
    if (webServer.args() > 0)
    {
        Serial.print("?");
        for (uint8_t i = 0; i < webServer.args(); i++)
        {
            if (! found && webServer.argName(i) == "iden" && webServer.arg(i).length() <= 3)
            {
                // Invalid conversion results in 0, which is ok: it corresponds to "dump all packets"
                serialDumpFilter = strtol(webServer.arg(i).c_str(), NULL, 16);
                found = true;
            } // if

            Serial.print(webServer.argName(i));
            Serial.print(F("="));
            Serial.print(webServer.arg(i));
            if (i < webServer.args() - 1) Serial.print('&');
        } // for
    } // if
    Serial.println(F("'"));

    webServer.send(200, F("text/plain"),
        ! found ? F("NOT OK!") :
            serialDumpFilter == 0 ?
            F("OK: dumping all JSON data") :
            F("OK: filtering JSON data"));
} // handleDumpFilter

// -----
// Fonts

// Defined in ArialRoundedMTbold.woff.ino
extern char ArialRoundedMTbold_woff[];
extern unsigned int ArialRoundedMTbold_woff_len;

// Defined in DotsAllForNow.woff.ino
extern char DotsAllForNow_woff[];
extern unsigned int DotsAllForNow_woff_len;

// Defined in DSEG7Classic-BoldItalic.woff.ino
extern char DSEG7Classic_BoldItalic_woff[];
extern unsigned int DSEG7Classic_BoldItalic_woff_len;

// Defined in DSEG14Classic-BoldItalic.woff.ino
extern char DSEG14Classic_BoldItalic_woff[];
extern unsigned int DSEG14Classic_BoldItalic_woff_len;

// Defined in fa-solid-900.woff.ino
extern char webfonts_fa_solid_900_woff[];
extern unsigned int webfonts_fa_solid_900_woff_len;

extern char jQuery_js[];  // Defined in jquery-3.5.1.min.js.ino
extern char mfd_js[];  // Defined in MFD.js.ino

// -----
// Cascading style sheet files

extern char faAll_css[];  // Defined in fa-all.css.ino
extern char carInfo_css[];  // Defined in CarInfo.css.ino

// -----
// HTML files

extern char mfd_html[];  // Defined in MFD.html.ino

// Print all HTTP request details on Serial
void printHttpRequest()
{
    Serial.print(F("[webServer] Received request from "));
    String ip = webServer.client().remoteIP().toString();
    Serial.print(ip);
    Serial.print(webServer.method() == HTTP_GET ? F(": GET - '") : F(": POST - '"));
    Serial.print(webServer.uri());

    if (webServer.args() > 0) Serial.print("?");

    for (uint8_t i = 0; i < webServer.args(); i++)
    {
        Serial.print(webServer.argName(i));
        Serial.print(F("="));
        Serial.print(webServer.arg(i));
        if (i < webServer.args() - 1) Serial.print('&');
    } // for

    Serial.println(F("'"));
} // printHttpRequest

// MIME type string constants
static const char PROGMEM textJavascriptStr[] = "text/javascript";
static const char PROGMEM textCssStr[] = "text/css";
static const char PROGMEM fontWoffStr[] = "font/woff";

// Serve a specified font
void serveFont(const char* urlPath, const char* content, unsigned int content_len)
{
    printHttpRequest();
    unsigned long start = millis();

    // Cache 10 seconds
    // TODO - does this header have any effect? Implementation of font caching seems pretty weird in various browsers...
    //webServer.sendHeader(F("Cache-Control"), F("private, max-age=10"), true);

    webServer.send_P(200, fontWoffStr, content, content_len);  
    Serial.printf_P(PSTR("[webServer] Sending '%S' took: %lu msec\n"), urlPath, millis() - start);
} // serveFont

// Serve a specified page (text, html, css, javascript, ...)
void servePage(const char* urlPath, const char* mimeType, const char* content)
{
    printHttpRequest();
    unsigned long start = millis();
    if (! checkETag(md5Checksum))
    {
        // Cache 10 seconds, then falls back to using the "If-None-Match" mechanism
        webServer.sendHeader(F("Cache-Control"), F("private, max-age=10"), true);
        webServer.send_P(200, mimeType, content);  
    } // if
    Serial.printf_P(PSTR("[webServer] Sending '%S' took: %lu msec\n"), urlPath, millis() - start);
} // servePage

// Serve the main HTML page
void serveMainHtml()
{
    servePage(PSTR("/MFD.html"), PSTR("text/html"), mfd_html);
} // serveMainHtml

void handleNotFound()
{
    if (! webServer.client().remoteIP().isSet()) return;  // No use to reply if there is no IP to reply to

#ifdef WIFI_AP_MODE
    // If webServer.uri() ends with '.html' or '.txt', then serve the main HTML page ('/MFD.html').
    // Useful for browsers that try to detect a captive portal, e.g. Firefox tries to browse to
    // http://detectportal.firefox.com/success.txt
    if (webServer.uri().endsWith(".html") || webServer.uri().endsWith(".txt"))
    {
        return serveMainHtml();
    } // if
#endif

    // Gold-plated response
    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += webServer.uri();
    message += F("\nMethod: ");
    message += (webServer.method() == HTTP_GET) ? F("GET") : F("POST");
    message += F("\nArguments: ");
    message += webServer.args();
    message += F("\n");
    for (uint8_t i = 0; i < webServer.args(); i++)
    {
        message += " " + webServer.argName(i) + F(": ") + webServer.arg(i) + F("\n");
    } // for

    webServer.send(404, F("text/plain;charset=utf-8"), message);
} // handleNotFound

// Results returned from the IR decoder
typedef struct
{
    unsigned long value;  // Decoded value
    int bits;  // Number of bits in decoded value
    volatile unsigned int *rawbuf;  // Raw intervals in 50 usec ticks
    int rawlen;  // Number of records in rawbuf
} TIrPacket;

// Defined in Wifi.ino
void setupWifi();
const char* WifiDataToJson(IPAddress& client);

// Defined in IRrecv.ino
void irSetup();
const char* parseIrPacketToJson(TIrPacket& pkt);
bool irReceive(TIrPacket& irPacket);

// Defined in PacketToJson.ino
const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt);
const char* EquipmentStatusDataToJson(char* buf, const int n);
void PrintJsonText(const char* jsonBuffer);

// Broadcast a (JSON) message to all the clients on the websocket
void BroadcastJsonText(const char* json)
{
    if (strlen(json) <= 0) return;

    delay(1); // Give some time to system to process other things?

    unsigned long start = millis();

    webSocket.broadcastTXT(json);

    // Print a message if the websocket broadcast took outrageously long (normally it takes around 1-2 msec).
    // If that takes really long (seconds or more), the VAN bus Rx queue will overrun.
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

void WebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
    switch(type)
    {
        case WStype_DISCONNECTED:
        {
            Serial.printf("Websocket [%u] Disconnected!\n", num);
        }
        break;

        case WStype_CONNECTED:
        {
            IPAddress clientIp = webSocket.remoteIP(num);
            Serial.printf("Websocket [%u] Connected from %d.%d.%d.%d url: %s\n",
                num,
                clientIp[0], clientIp[1], clientIp[2], clientIp[3],
                payload);

            // Dump ESP system data
            BroadcastJsonText(EspDataToJson());

            // Dump Wi-Fi and IP data
            BroadcastJsonText(WifiDataToJson(clientIp));

            // Dump equipment status data, e.g. presence of sat nav and other devices
            #define EQPT_STATUS_DATA_JSON_BUFFER_SIZE 2048
            char jsonBuffer[EQPT_STATUS_DATA_JSON_BUFFER_SIZE];
            BroadcastJsonText(EquipmentStatusDataToJson(jsonBuffer, EQPT_STATUS_DATA_JSON_BUFFER_SIZE));
        }
        break;
    } // switch
} // WebSocketEvent

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    delay(1000);
    Serial.begin(115200);
    Serial.println(F("Starting VAN bus \"Live Connect\" server"));

    PrintSystemSpecs();

    setupWifi();

#ifdef WIFI_AP_MODE
    // If DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);
#endif // WIFI_AP_MODE

    // Setup "Over The Air" (OTA) update
    setupOta();

    // Setup HTML server

    // Fonts
    webServer.on(F("/ArialRoundedMTbold.woff"), [](){
        serveFont(PSTR("/ArialRoundedMTbold.woff"), ArialRoundedMTbold_woff, ArialRoundedMTbold_woff_len);
    });
    webServer.on(F("/DotsAllForNow.woff"), [](){
        serveFont(PSTR("/DotsAllForNow.woff"), DotsAllForNow_woff, DotsAllForNow_woff_len);
    });
    webServer.on(F("/DSEG7Classic-BoldItalic.woff"), [](){
        serveFont(PSTR("/DSEG7Classic-BoldItalic.woff"), DSEG7Classic_BoldItalic_woff, DSEG7Classic_BoldItalic_woff_len);
    });
    webServer.on(F("/DSEG14Classic-BoldItalic.woff"), [](){
        serveFont(PSTR("/DSEG14Classic-BoldItalic.woff"), DSEG14Classic_BoldItalic_woff, DSEG14Classic_BoldItalic_woff_len);
    });
    webServer.on(F("/webfonts/fa-solid-900.woff"), [](){
        serveFont(PSTR("/webfonts/fa-solid-900.woff"), webfonts_fa_solid_900_woff, webfonts_fa_solid_900_woff_len);
    });

    // Javascript files
    webServer.on(F("/jquery-3.5.1.min.js"), [](){
        servePage(PSTR("/jquery-3.5.1.min.js"), textJavascriptStr, jQuery_js);
    });
    webServer.on(F("/MFD.js"), [](){
        servePage(PSTR("/MFD.js"), textJavascriptStr, mfd_js);
    });

    // Cascading style sheet files
    webServer.on(F("/css/all.css"), [](){
        servePage(PSTR("/css/all.css"), textCssStr, faAll_css);
    });
    webServer.on(F("/CarInfo.css"), [](){
        servePage(PSTR("/CarInfo.css"), textCssStr, carInfo_css);
    });

    // HTML files
    webServer.on(F("/MFD.html"), serveMainHtml);

    webServer.on("/dumpOnly", handleDumpFilter);

    webServer.onNotFound(handleNotFound);

    const char* headers[] = {"If-None-Match"};
    webServer.collectHeaders(headers, sizeof(headers)/ sizeof(headers[0]));

    webServer.begin();

    webSocket.begin();
    webSocket.onEvent(WebSocketEvent);

    Serial.print(F("Please surf to: http://"));
#ifdef WIFI_AP_MODE
    Serial.print(apIP);
#else
    Serial.print(WiFi.localIP());
#endif // WIFI_AP_MODE
    Serial.println(F("/MFD.html"));

    // Having the default VAN packet queue size of 15 (see VanBusRx.h) seems too little given the time that
    // is needed to send a JSON packet over the Wi-Fi; seeing quite some "VAN PACKET QUEUE OVERRUN!" lines.
    // Looks like it should be set to at least 100.
    #define VAN_PACKET_QUEUE_SIZE 100
    VanBusRx.Setup(RX_PIN, VAN_PACKET_QUEUE_SIZE);
    Serial.printf_P(PSTR("VanBusRx queue of size %d is set up\n"), VanBusRx.QueueSize());

    irSetup();
} // setup

void loop()
{
#ifdef WIFI_AP_MODE
    dnsServer.processNextRequest();
#endif // WIFI_AP_MODE

    loopOta(); 

    webSocket.loop();
    webServer.handleClient();

    // IR receiver
    TIrPacket irPacket;
    if (irReceive(irPacket)) BroadcastJsonText(parseIrPacketToJson(irPacket));

    // VAN bus receiver
    TVanPacketRxDesc pkt;
    bool isQueueOverrun = false;
    if (VanBusRx.Receive(pkt, &isQueueOverrun)) BroadcastJsonText(ParseVanPacketToJson(pkt));
    if (isQueueOverrun) Serial.print(F("VAN PACKET QUEUE OVERRUN!\n"));

    // Print statistics every 5 seconds
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 5000UL) // Arithmetic has safe roll-over
    {
        lastUpdate = millis();
        VanBusRx.DumpStats(Serial);
    } // if

    // Handle persistent storage
    LoopStore();
} // loop
