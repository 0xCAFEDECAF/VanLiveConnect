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

// Uncomment to see JSON buffers printed on the Serial port.
// Note: printing the JSON buffers takes pretty long, so it leads to more Rx queue overruns.
#define PRINT_JSON_BUFFERS_ON_SERIAL

#include <ESP8266WiFi.h>

// Either this for "WebSockets" (https://github.com/Links2004/arduinoWebSockets):
#include <WebSocketsServer.h>

// Or this for "WebSockets_Generic" (https://github.com/khoih-prog/WebSockets_Generic):
//#include <WebSocketsServer_Generic.h>

#include <ESP8266WebServer.h>
#include <VanBusRx.h>

#include "Config.h"

// Over-the-air (OTA) update
void setupOta();
void loopOta();

#if defined ARDUINO_ESP8266_GENERIC || defined ARDUINO_ESP8266_ESP01
// For ESP-01 board we use GPIO 2 (internal pull-up, keep disconnected or high at boot time)
#define D2 (2)
#define LED_BUILTIN (13)
#endif

int RX_PIN = D2; // Set to GPIO pin connected to VAN bus transceiver output

#include <DNSServer.h>

#ifdef WIFI_AP_MODE

#define DNS_PORT (53)
DNSServer dnsServer;
IPAddress apIP(172, 217, 28, 1);

#endif // WIFI_AP_MODE

// Encourage the compiler to use the a newer (C++11 ?) standard. If this is removed, it doesn't compile!
char dummy_var_to_use_cpp11[] PROGMEM = R"==(")==";

ESP8266WebServer webServer;
WebSocketsServer webSocket = WebSocketsServer(81);

// Defined in Esp.ino
extern const String md5Checksum;

// Returns true if the actual Etag is equal to the received Etag in If-None-Match header field.
// Shamelessly copied from: https://werner.rothschopf.net/microcontroller/202011_arduino_webserver_caching_en.htm .
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
    // webServer.sendHeader("Cache-Control", "public, max-age=2678400"); // cache 31 days
    webServer.sendHeader(F("Cache-Control"), F("private, max-age=31536000")); // cache 365 days
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

// Defined in fa-regular-400.woff.ino
extern char webfonts_fa_regular_400_woff[];
extern unsigned int webfonts_fa_regular_400_woff_len;

extern char jQuery_js[];  // Defined in jquery-3.5.1.min.js.ino
extern char mfd_js[];  // Defined in MFD.js.ino

// Cascading style sheet files
extern char faAll_css[];  // Defined in fa-all.css.ino
extern char carInfo_css[];  // Defined in CarInfo.css.ino

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
    //webServer.sendHeader(F("Cache-Control"), F("public"), true);
    webServer.sendHeader(F("Cache-Control"), F("private, max-age=31536000"), true); // cache 365 days
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
        //webServer.sendHeader(F("Cache-Control"), F("public"), true);
        webServer.sendHeader(F("Cache-Control"), F("private, max-age=31536000"), true); // cache 365 days
        webServer.send_P(200, mimeType, content);  
    } // if
    Serial.printf_P(PSTR("[webServer] Sending '%S' took: %lu msec\n"), urlPath, millis() - start);
} // servePage

// Serve the main HTML page
void serveMainHtml()
{
    servePage(PSTR("/MFD.html"), PSTR("text/html"), mfd_html);
} // serveMainHtml

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

// Defined in IRrecv.ino
void irSetup();
const char* parseIrPacketToJson(TIrPacket& pkt);
bool irReceive(TIrPacket& irPacket);

// Defined in Esp.ino
void printSystemSpecs();
const char* espDataToJson();

// Defined in PacketToJson.ino
const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt);
void PrintJsonText(const char* jsonBuffer);

void broadcastJsonText(const char* json)
{
    if (strlen(json) > 0)
    {
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
    } // if
} // broadcastJsonText

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
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
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("Websocket [%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // Dump some system data
            broadcastJsonText(espDataToJson());
        }
        break;
    } // switch
} // webSocketEvent

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    delay(1000);
    Serial.begin(115200);
    Serial.println(F("Starting VAN bus \"Live Connect\" server"));

    printSystemSpecs();

    setupWifi();

#ifdef WIFI_AP_MODE
    // If DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);
#endif // WIFI_AP_MODE

    // Setup Over-The-Air update
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
    webServer.on(F("/webfonts/fa-regular-400.woff"), [](){
        serveFont(PSTR("/webfonts/fa-regular-400.woff"), webfonts_fa_regular_400_woff, webfonts_fa_regular_400_woff_len);
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

    // Reply to all other requests with '/MFD.html'
    webServer.onNotFound(serveMainHtml);

    webServer.on("/dumpOnly", handleDumpFilter);

    const char* headers[] = {"If-None-Match"};
    webServer.collectHeaders(headers, sizeof(headers)/ sizeof(headers[0]));

    webServer.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

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
    if (irReceive(irPacket)) broadcastJsonText(parseIrPacketToJson(irPacket));

    // VAN bus receiver
    TVanPacketRxDesc pkt;
    bool isQueueOverrun = false;
    if (VanBusRx.Receive(pkt, &isQueueOverrun)) broadcastJsonText(ParseVanPacketToJson(pkt));
    if (isQueueOverrun) Serial.print(F("VAN PACKET QUEUE OVERRUN!\n"));

    // Print statistics every 5 seconds
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 5000UL) // Arithmetic has safe roll-over
    {
        lastUpdate = millis();
        VanBusRx.DumpStats(Serial);
    } // if
} // loop
