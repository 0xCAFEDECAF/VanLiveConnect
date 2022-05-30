
#include <ESP8266WebServer.h>

#ifdef SERVE_FROM_SPIFFS

#include "FS.h"

// Use the following #defines to define which type of web documents will be served from the
// SPI flash file system (SPIFFS)
#define SERVE_MAIN_FILES_FROM_SPIFFS  // MFD.html, MFD.js
#define SERVE_FONTS_FROM_SPIFFS  // .woff files
#define SERVE_JAVASCRIPT_FROM_SPIFFS  // jquery-3.5.1.min.js
#define SERVE_CSS_FROM_SPIFFS  // all.css, CarInfo.css

#endif // SERVE_FROM_SPIFFS

ESP8266WebServer webServer;

// Defined in Esp.ino
extern const String md5Checksum;

#ifdef SERVE_FROM_SPIFFS

// Table with the MD5 hash value of each file in the root directory
struct FileMd5Entry_t
{
    String path;
    String md5;
}; // FileMd5Entry_t

#define MAX_FILE_MD5 20
FileMd5Entry_t fileMd5[MAX_FILE_MD5];
int fileMd5Last = 0;

String _formatBytes(size_t bytes)
{
    if (bytes < 1024) return String(bytes)+" Bytes";
    else if (bytes < (1024 * 1024)) return String(bytes/1024.0)+" KBytes";
    else if(bytes < (1024 * 1024 * 1024)) return String(bytes/1024.0/1024.0)+" MBytes";
    else return String(bytes/1024.0/1024.0/1024.0)+" GBytes";
} // _formatBytes

void SetupStore()
{
    Serial.print(F("Mounting SPI Flash File System (SPIFFS) ..."));

    VanBusRx.Disable();

    // Make sure the file system is formatted and mounted
    if (! SPIFFS.begin())
    {
        Serial.print(F("\nFailed to mount file system, trying to format ..."));
        if (! SPIFFS.format())
        {
            VanBusRx.Enable();
            Serial.println(F("\nFailed to format file system, no persistent storage available!"));
            return;
        } // if
    } // if

    // Print file system size
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    char b[MAX_FLOAT_SIZE];
    Serial.printf_P(PSTR(" OK, total %s MByes\n"), FloatToStr(b, fs_info.totalBytes / 1024.0 / 1024.0, 2));

    // Print the contents of the root directory
    bool foundOne = false;
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
        String fileName = dir.fileName();

        // Create a table with the MD5 hash value of each file
        // Inspired by https://github.com/esp8266/Arduino/issues/3003
        File file = SPIFFS.open(fileName, "r");
        size_t fileSize = file.size();
        MD5Builder md5;
        md5.begin();
        md5.addStream(file, fileSize);
        md5.calculate();
        md5.toString();
        file.close();

        Serial.printf_P(
            PSTR("FS File: '%s', size: %s, MD5: %s\n"),
            fileName.c_str(),
            _formatBytes(fileSize).c_str(),
            md5.toString().c_str()
        );

        fileMd5[fileMd5Last].path = fileName;
        fileMd5[fileMd5Last].md5 = md5.toString();
        if (++fileMd5Last >= MAX_FILE_MD5)
        {
            Serial.println("=====> Too many files found: please increase MAX_FILE_MD5");
            break;
        } // if
    } // while

    VanBusRx.Enable();
} // SetupStore

String getMd5(const String& path)
{
    int i = 0;
    while (i < MAX_FILE_MD5)
    {
        if (fileMd5[i].path == path) return fileMd5[i].md5;
        i++;
    } // while

    // To indicate 'path' not found
    return "";
} // getMd5

#endif // SERVE_FROM_SPIFFS

// Print all HTTP request details on Serial
void printHttpRequest()
{
  #ifdef DEBUG_WEBSERVER
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
  #endif // DEBUG_WEBSERVER
} // printHttpRequest

// Returns true if the actual Etag is equal to the received Etag in an 'If-None-Match' header field.
// Shameless copy from: https://werner.rothschopf.net/microcontroller/202011_arduino_webserver_caching_en.htm .
bool checkETag(const String& etag)
{
    if (etag == "") return false;

    for (int i = 0; i < webServer.headers(); i++)
    {
        // Serial.println(
            // String(F("[webServer] ")) + webServer.headerName(i) + F(": \"") + webServer.header(i) + F("\""));
        if (webServer.headerName(i).compareTo(F("If-None-Match")) == 0)
        {
            String read = webServer.header(i);
            read.replace("\"", ""); // some browsers (i.e. Samsung) discard the double quotes
            if (read == etag)
            {
                // Tells the client that it can cache the asset, but it cannot use the cached asset without
                // revalidating with the server
                webServer.sendHeader(F("Cache-Control"), F("no-cache"), true);

                webServer.send(304, "text/plain", F("Not Modified"));
              #ifdef DEBUG_WEBSERVER
                Serial.println(
                    String(F("[webServer] ")) + webServer.headerName(i) + F(": ") + webServer.header(i) + F(" - Not Modified"));
              #endif // DEBUG_WEBSERVER
                return true;
            } // if
        } // if
    } // for

    webServer.sendHeader(F("ETag"), String("\"") + etag + "\"");

    // Tells the client that it can cache the asset, but it cannot use the cached asset without revalidating with
    // the server
    webServer.sendHeader(F("Cache-Control"), F("no-cache"), true);

    return false;
} // checkETag

// serialDumpFilter == 0 means: no filtering; print all
// serialDumpFilter != 0 means: print only the packet + JSON data for the specified IDEN
uint16_t serialDumpFilter;

// Set a simple filter on the dumping of packet + JSON data on Serial.
// Surf to e.g. http://car.lan/dumpOnly?iden=8c4 to have only packets with IDEN 0x8C4 dumped on serial.
// Surf to http://car.lan/dumpOnly?iden=0 to remove any filter, i.e. dump all packets.
void HandleDumpFilter()
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
                // Invalid conversion results in 0, which is ok: it corresponds to "remove any filter"
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
        serialDumpFilter == 0 ? F("OK: dumping all JSON data") :
        F("OK: filtering JSON data"));
} // HandleDumpFilter

void HandleAndroidConnectivityCheck()
{
    printHttpRequest();
    unsigned long start = millis();

    webServer.send(204, "");

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("[webServer] Serving '%S' took: %lu msec\n"),
        webServer.uri().c_str(),
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // HandleAndroidConnectivityCheck

// -----
// MIME type string constants

//static const char PROGMEM textJavascriptStr[] = "text/javascript";
static const char textJavascriptStr[] = "text/javascript";
//static const char PROGMEM textCssStr[] = "text/css";
static const char textCssStr[] = "text/css";
//static const char PROGMEM fontWoffStr[] = "font/woff";
static const char fontWoffStr[] = "application/font-woff";

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

// -----
// Javascript files

extern char jQuery_js[];  // Defined in jquery-3.5.1.min.js.ino
extern char mfd_js[];  // Defined in MFD.js.ino

// -----
// Cascading style sheet files

extern char faAll_css[];  // Defined in fa-all.css.ino
extern char carInfo_css[];  // Defined in CarInfo.css.ino

// -----
// HTML files

extern char mfd_html[];  // Defined in MFD.html.ino

void HandleNotFound()
{
    printHttpRequest();

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("[webServer] File '%s' not found\n"), webServer.uri().c_str());
  #endif // DEBUG_WEBSERVER

    if (! webServer.client().remoteIP().isSet()) return;  // No use to reply if there is no IP to reply to

  #ifdef WIFI_AP_MODE
    // Redirect to the main HTML page ('/MFD.html').
    // Useful for browsers that try to detect a captive portal, e.g. Firefox tries to browse to
    // http://detectportal.firefox.com/success.txt ; Android tries to load https://www.gstatic.com/generate_204 .
    webServer.sendHeader(F("Location"), F("http://" IP_ADDR "/MFD.html"), true);
    //webServer.sendHeader(F("Cache-Control"), F("no-store"), true); // TODO - necessary?
    //webServer.send(301, F("text/plain"), F("Redirect"));
    webServer.send(302, F("text/plain"), F("Found"));
    return;
  #endif // WIFI_AP_MODE

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
} // HandleNotFound

// Serve a specified font from program memory
void ServeFont(PGM_P content, unsigned int content_len)
{
    printHttpRequest();
    unsigned long start = millis();

    webServer.send_P(200, fontWoffStr, content, content_len);

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("[webServer] Serving font '%S' took: %lu msec\n"),
        webServer.uri().c_str(),
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // ServeFont

#ifdef SERVE_FROM_SPIFFS

// Serve a specified font from the SPI Flash File System (SPIFFS)
void ServeFontFromFile(const char* path)
{
    unsigned long start = millis();

    VanBusRx.Disable();
    if (! SPIFFS.exists(path))
    {
        VanBusRx.Enable();
        return HandleNotFound();
    } // if

    printHttpRequest();

    File file = SPIFFS.open(path, "r");
    size_t sent = webServer.streamFile(file, fontWoffStr);
    file.close();
    VanBusRx.Enable();

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("[webServer] Serving font '%s' from file system took: %lu msec\n"),
        webServer.uri().c_str(),
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // ServeFontFromFile

#endif // SERVE_FROM_SPIFFS

// Serve a specified document (text, html, css, javascript, ...) from program memory
void ServeDocument(PGM_P mimeType, PGM_P content)
{
    printHttpRequest();
    unsigned long start = millis();
    bool eTagMatches = checkETag(md5Checksum);
    if (! eTagMatches)
    {
        // Serve the complete document
        webServer.send_P(200, mimeType, content);
    } // if

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("[webServer] %S '%s' took: %lu msec\n"),
        eTagMatches ? PSTR("Responding to request for") : PSTR("Serving"),
        webServer.uri().c_str(),
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // ServeDocument

// Convert the file extension to the MIME type
const char* getContentType(const String& path)
{
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".woff")) return fontWoffStr;
    if (path.endsWith(".css")) return textCssStr;
    if (path.endsWith(".js")) return textJavascriptStr;
    if (path.endsWith(".ico")) return "image/x-icon";
    if (path.endsWith(".jpg")) return "image/jpeg";
    if (path.endsWith(".png")) return "image/png";
    return "text/plain";
} // getContentType

#ifdef SERVE_FROM_SPIFFS

// Serve a specified document (text, html, css, javascript, ...) from the SPI Flash File System (SPIFFS)
void ServeDocumentFromFile(const char* urlPath = 0, const char* mimeType = 0)
{
    String path(urlPath == 0 ? webServer.uri() : urlPath);
    String fsPath = path;
    String md5 = getMd5(path);
    if (md5.length() == 0)
    {
        // If 'getMd5(path)' returns an empty string, it means that 'path' is not found in the file system

        // Try the ".gz" file
        md5 = getMd5(path + ".gz");
        if (md5.length() == 0)
        {
          // #ifdef DEBUG_WEBSERVER
            // Serial.printf_P(PSTR("[webServer] File '%s' not found\n"), path.c_str());
          // #endif // DEBUG_WEBSERVER
            return HandleNotFound();
        } // if

        fsPath += ".gz";
    } // if

    printHttpRequest();

    unsigned long start = millis();

    bool eTagMatches = checkETag(md5);
    if (! eTagMatches)
    {
        // Get the MIME type, if necessary
        if (mimeType == 0) mimeType = getContentType(path);

        // Serve the complete document
        VanBusRx.Disable();
        File file = SPIFFS.open(fsPath, "r");
        size_t sent = webServer.streamFile(file, mimeType);
        file.close();
        VanBusRx.Enable();
    } // if

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("[webServer] %S '%S' from file system took: %lu msec\n"),
        eTagMatches ? PSTR("Responding to request for") : PSTR("Serving"),
        path.c_str(),
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // ServeDocumentFromFile

#endif // SERVE_FROM_SPIFFS

// Serve the main HTML page
void ServeMainHtml()
{
  #ifdef SERVE_MAIN_FILES_FROM_SPIFFS

    // Serve from the SPI flash file system
    ServeDocumentFromFile("/MFD.html");

  #else

    // Serve from program memory, so updating is easy and does not need the SPI flash file system uploader
    ServeDocument(PSTR("text/html"), mfd_html);

  #endif // SERVE_MAIN_FILES_FROM_SPIFFS
} // ServeMainHtml

void SetupWebServer()
{
    // -----
    // Fonts

  #ifdef SERVE_FONTS_FROM_SPIFFS

    webServer.on(F("/ArialRoundedMTbold.woff"), [](){ ServeFontFromFile("/ArialRoundedMTbold.woff"); });
    webServer.on(F("/DotsAllForNow.woff"), [](){ ServeFontFromFile("/DotsAllForNow.woff"); });
    webServer.on(F("/DSEG7Classic-BoldItalic.woff"), [](){ ServeFontFromFile("/DSEG7Classic-BoldItalic.woff"); });
    webServer.on(F("/DSEG14Classic-BoldItalic.woff"), [](){ ServeFontFromFile("/DSEG14Classic-BoldItalic.woff"); });
    webServer.on(F("/webfonts/fa-solid-900.woff"), [](){ ServeFontFromFile("/fa-solid-900.woff"); });

  #else

    webServer.on(F("/ArialRoundedMTbold.woff"), [](){
        ServeFont(ArialRoundedMTbold_woff, ArialRoundedMTbold_woff_len);
    });
    webServer.on(F("/DotsAllForNow.woff"), [](){
        ServeFont(DotsAllForNow_woff, DotsAllForNow_woff_len);
    });
    webServer.on(F("/DSEG7Classic-BoldItalic.woff"), [](){
        ServeFont(DSEG7Classic_BoldItalic_woff, DSEG7Classic_BoldItalic_woff_len);
    });
    webServer.on(F("/DSEG14Classic-BoldItalic.woff"), [](){
        ServeFont(DSEG14Classic_BoldItalic_woff, DSEG14Classic_BoldItalic_woff_len);
    });
    webServer.on(F("/webfonts/fa-solid-900.woff"), [](){
        ServeFont(webfonts_fa_solid_900_woff, webfonts_fa_solid_900_woff_len);
    });

  #endif // SERVE_FONTS_FROM_SPIFFS

    // -----
    // Javascript files

  #ifdef SERVE_JAVASCRIPT_FROM_SPIFFS

    webServer.on(F("/jquery-3.5.1.min.js"), [](){ ServeDocumentFromFile(); });

  #else

    webServer.on(F("/jquery-3.5.1.min.js"), [](){ ServeDocument(textJavascriptStr, jQuery_js); });

  #endif // SERVE_JAVASCRIPT_FROM_SPIFFS

  #ifdef SERVE_MAIN_FILES_FROM_SPIFFS

    webServer.on(F("/MFD.js"), [](){ ServeDocumentFromFile(); });

  #else

    webServer.on(F("/MFD.js"), [](){ ServeDocument(textJavascriptStr, mfd_js); });

  #endif // SERVE_MAIN_FILES_FROM_SPIFFS

    // -----
    // Cascading style sheet files

  #ifdef SERVE_CSS_FROM_SPIFFS

    webServer.on(F("/css/all.css"), [](){ ServeDocumentFromFile("/all.css"); });
    webServer.on(F("/CarInfo.css"), [](){ ServeDocumentFromFile(); });

  #else

    webServer.on(F("/css/all.css"), [](){ ServeDocument(textCssStr, faAll_css); });
    webServer.on(F("/CarInfo.css"), [](){ ServeDocument(textCssStr, carInfo_css); });

  #endif // SERVE_CSS_FROM_SPIFFS

    // -----
    // HTML files

    webServer.on(F("/MFD.html"), ServeMainHtml);

    // -----
    // Miscellaneous

    webServer.on(F("/dumpOnly"), HandleDumpFilter);
    webServer.on(F("/generate_204"), HandleAndroidConnectivityCheck);
    webServer.on(F("/gen_204"), HandleAndroidConnectivityCheck);

  #ifdef SERVE_FROM_SPIFFS

    // Try to serve any not further listed document from the SPI flash file system
    webServer.onNotFound([](){ ServeDocumentFromFile(); });

  #else

    webServer.onNotFound(HandleNotFound);

  #endif // SERVE_FROM_SPIFFS

    const char* headers[] = { "If-None-Match" };
    webServer.collectHeaders(headers, sizeof(headers)/ sizeof(headers[0]));

    webServer.begin();
} // SetupWebServer

void LoopWebServer()
{
    webServer.handleClient();
} // LoopWebServer
