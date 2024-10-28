
#include <map>
#include <ESPAsyncWebSrv.h>  // https://github.com/dvarrel/ESPAsyncWebSrv

#if defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

#include "FS.h"

#ifdef SERVE_FROM_LITTLEFS
  #include <LittleFS.h>
  #define SPIFFS LittleFS
#endif

// Use the following #defines to define which type of web documents will be served from the
// flash file system (SPIFFS or LittleFS)
#define SERVE_MAIN_FILES_FROM_FFS  // MFD.html, MFD.js

// Note: it seems to be better to not serve font files from the FFS. The browser requests these files very
// late, at seemingly random times after the initial page requests. I've seen ESP often running out of stack
// space while serving a font file.

//#define SERVE_FONTS_FROM_FFS  // .woff files

#define SERVE_JAVASCRIPT_FROM_FFS  // jquery-3.5.1.min.js
#define SERVE_CSS_FROM_FFS  // all.css, CarInfo.css

#endif // defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

// Create AsyncWebServer on port 80
AsyncWebServer webServer(80);

// Defined in Esp.ino
extern const String md5Checksum;

// Defined in WebSocket.ino
extern std::map<uint32_t, unsigned long> lastWebSocketCommunication;
void DeleteAllQueuedJsons();

#if defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

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
  #ifdef SERVE_FROM_SPIFFS
    Serial.print(F("Mounting SPI Flash File System (SPIFFS) ..."));
  #else
    Serial.print(F("Mounting LittleFS flash file system ..."));
  #endif // SERVE_FROM_SPIFFS

    VanBusRx.Disable();

    // Make sure the file system is formatted and mounted
    if (! SPIFFS.begin())
    {
        Serial.print(F("\nFailed to mount file system, trying to format ..."));
        if (! SPIFFS.format())
        {
            VanBusRx.Enable();
            Serial.print(F("\nFailed to format file system, no persistent storage available!\n"));
            return;
        } // if
    } // if

    // Print file system size
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    char b[MAX_FLOAT_SIZE];
    Serial.printf_P(PSTR(" OK, total %s MByes\n"), FloatToStr(b, fs_info.totalBytes / 1024.0 / 1024.0, 2));

    // Print the contents of the root directory
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
      #ifdef SERVE_FROM_LITTLEFS
        String fileName = "/" + dir.fileName();
      #else
        String fileName = dir.fileName();
      #endif // SERVE_FROM_LITTLEFS

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
            Serial.print("====> Too many files found: please increase MAX_FILE_MD5\n");
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

#endif // defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

// Print all HTTP request details on Serial
void printHttpRequest(class AsyncWebServerRequest* request)
{
  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("%s[webServer] Received request from "), TimeStamp());
    Serial.print(request->client()->remoteIP());
    Serial.printf_P(PSTR(": %s - 'http://"), request->methodToString());
    Serial.print(request->host());
    Serial.print(request->url());

    if (request->args() > 0) Serial.print("?");

    for (size_t i = 0; i < request->args(); i++)
    {
        Serial.print(request->argName(i));
        Serial.print("=");
        Serial.print(request->arg(i));
        if (i < request->args() - 1) Serial.print('&');
    } // for

    Serial.print("'\n");
  #else
    (void)request;
  #endif // DEBUG_WEBSERVER
} // printHttpRequest

// Returns true if the actual Etag is equal to the received Etag in an 'If-None-Match' header field.
// Shameless copy from: https://werner.rothschopf.net/microcontroller/202011_arduino_webserver_caching_en.htm .
bool checkETag(class AsyncWebServerRequest* request, const String& etag)
{
  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("%s[webServer] checkETag(%s)\n"), TimeStamp(), etag.c_str());
  #endif // DEBUG_WEBSERVER

    if (etag == "") return false;

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("%s[webServer] request->headers = %lu\n"), TimeStamp(), request->headers());

    Serial.printf_P(PSTR("%s[webServer] all request headers:\n"), TimeStamp());
    for (int i = 0; i < 100; i++)
    {
        String headerName = request->headerName(i);
        if (headerName.length() == 0) continue;
        Serial.printf_P(PSTR("               - %s : %s\n"), headerName.c_str(), request->header(i).c_str());
    } // for
  #endif // DEBUG_WEBSERVER

    if(request->hasHeader(F("If-None-Match")))
    {
        String read = request->header("If-None-Match");

        read.replace("\"", ""); // some browsers (i.e. Samsung) discard the double quotes
        if (read == etag)
        {
            DeleteAllQueuedJsons();  // Maximize free heap space

            AsyncWebServerResponse* response = request->beginResponse(304, F("text/plain"), F("Not Modified"));

            // This needs to be repeated; see https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-None-Match :
            //   "Note that the server generating a 304 response MUST generate any of the following header fields
            //   that would have been sent in a 200 (OK) response to the same request: Cache-Control, Content-Location,
            //   Date, ETag, Expires, and Vary."
            response->addHeader(F("Cache-Control"), F("no-cache"));
            response->addHeader("ETag", String("\"") + etag + "\"");
            request->send(response);

          #ifdef DEBUG_WEBSERVER
            Serial.printf_P(PSTR("%s"), TimeStamp());
            Serial.print(F("[webServer] If-None-Match: "));
            Serial.print(etag);
            Serial.print(F(" - Not Modified\n"));
          #endif // DEBUG_WEBSERVER
            return true;
        } // if
    } // if

    return false;
} // checkETag

void HandleAndroidConnectivityCheck(class AsyncWebServerRequest* request)
{
    printHttpRequest(request);

    // After the WebSocket connection is established, no longer respond to connectivity check. In that way,
    // Android knows (after no longer getting responses on '/generate_204') that this Wi-Fi is not providing
    // an Internet connection and will try to re-establish Internet connectivity via mobile data.
    //
    // Notes:
    // - In Android, go to Settings --> Network & Internet --> Wi-Fi --> Wi-Fi preferences --> Advanced -->
    //   then turn on "Switch to mobile data automatically"; see the screen shot at:
    //   https://wmstatic.global.ssl.fastly.net/ml/7090822-f-91188da7-6fcb-4c48-9052-0b2b108e7f54.png
    //   (TODO - check if is this really needed?)
    // - In Android, setting "Mobile data always active" inside "Developer Options" is not necessary
    //   (and even undesired to save battery).
    // - The WebSocket (TCP/IP) connection will persist, even after Android switches from Wi-Fi to mobile data.

    IPAddress clientIp = request->client()->remoteIP();

    if (lastWebSocketCommunication.find(clientIp) != lastWebSocketCommunication.end())
    {
        unsigned long since = millis() - lastWebSocketCommunication[clientIp];  // Arithmetic has safe roll-over

      #define WEBSERVER_RESPOND_TO_204_AFTER_MS (7 * 1000)

      #ifdef DEBUG_WEBSERVER
        Serial.printf_P(PSTR("%s[webServer] Last websocket communication with %s was %lu msecs ago: %sresponding\n"),
            TimeStamp(),
            clientIp.toString().c_str(),
            since,
            since < WEBSERVER_RESPOND_TO_204_AFTER_MS ? PSTR("NOT ") : emptyStr
        );
      #endif // DEBUG_WEBSERVER

        if (since < WEBSERVER_RESPOND_TO_204_AFTER_MS) return;
    } // if

    // As long as the WebSocket connection is not established, respond to connectivity check. In that way,
    // the browser will use this network connection to load the '/MFD.html' page from, and subsequently
    // connect via the WebSocket.

  #ifdef DEBUG_WEBSERVER
    unsigned long start = millis();
  #endif // DEBUG_WEBSERVER

    request->send(204);

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("%s[webServer] Serving '%s' took: %lu msec\n"),
        TimeStamp(),
        request->url().c_str(),
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // HandleAndroidConnectivityCheck

// -----
// MIME type string constants

//static const char PROGMEM textJavaScriptStr[] = "text/javascript";
static const char textJavaScriptStr[] = "text/javascript";
//static const char PROGMEM textCssStr[] = "text/css";
static const char textCssStr[] = "text/css";
//static const char PROGMEM fontWoffStr[] = "font/woff";
static const char fontWoffStr[] = "application/font-woff";

// -----
// Fonts

// Defined in PeugeotNewRegular.woff.woff.ino
extern char PeugeotNewRegular_woff[];
extern unsigned int PeugeotNewRegular_woff_len;

// Defined in fa-solid-900.woff.ino
extern char webfonts_fa_solid_900_woff[];
extern unsigned int webfonts_fa_solid_900_woff_len;

// -----
// JavaScript files

extern char jQuery_js[];  // Defined in jquery-3.5.1.min.js.ino
extern char mfd_js[];  // Defined in MFD.js.ino

// -----
// Cascading style sheet files

extern char faAll_css[];  // Defined in fa-all.css.ino
extern char carInfo_css[];  // Defined in CarInfo.css.ino

// -----
// HTML files

extern char mfd_html[];  // Defined in MFD.html.ino

void HandleNotFound(class AsyncWebServerRequest* request)
{
    printHttpRequest(request);

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("%s[webServer] File '%s' not found\n"), TimeStamp(), request->url().c_str());
  #endif // DEBUG_WEBSERVER

    if (! request->client()->remoteIP()) return;  // No use to reply if there is no IP to reply to

  #ifdef WIFI_AP_MODE // Wi-Fi access point mode
    #define MY_IP String(IP_ADDR)
  #else
    #define MY_IP  WiFi.localIP().toString()
  #endif // ifdef WIFI_AP_MODE

  #ifdef WIFI_AP_MODE
    // Redirect to the main HTML page ('/MFD.html').
    // Useful for browsers that try to detect a captive portal, e.g. Firefox tries to browse to
    // http://detectportal.firefox.com/success.txt and http://detectportal.firefox.com/canonical.html .

    // TODO - commented out because this occasionally crashes the ESP due to out-of-memory condition
  #if 0
    DeleteAllQueuedJsons();  // Maximize free heap space

    AsyncWebServerResponse* response = request->beginResponse(302, F("text/plain"), F("Found"));
    response->addHeader(F("Location"), "http://" + MY_IP + "/MFD.html");
    request->send(response);

    return;
  #endif // 0
  #endif // WIFI_AP_MODE

    if (request->url() == "/")
    {
        // Redirect to the main HTML page ('/MFD.html').

        DeleteAllQueuedJsons();  // Maximize free heap space

        AsyncWebServerResponse* response = request->beginResponse(302, F("text/plain"), F("Found"));
        response->addHeader(F("Location"), "http://" + MY_IP + "/MFD.html");
        request->send(response);

      #ifdef DEBUG_WEBSERVER
        Serial.printf_P(PSTR("%s[webServer] Redirected (302) to 'http://%s/MFD.html'\n"), TimeStamp(), MY_IP.c_str());
      #endif // DEBUG_WEBSERVER

        return;
    } // if

    // Gold-plated response
    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += request->url();
    message += F("\nMethod: ");
    message += request->methodToString();
    message += F("\nArguments: ");
    message += request->args();
    message += "\n";
    for (size_t i = 0; i < request->args(); i++)
    {
        message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
    } // for

    request->send(404, F("text/plain;charset=utf-8"), message);
} // HandleNotFound

// Serve a specified font from program memory
void ServeFont(class AsyncWebServerRequest* request, const char* content, size_t content_len)
{
    printHttpRequest(request);

    // Skip Etag checking; browsers don't seem to use that when requesting fonts

    DeleteAllQueuedJsons();  // Maximize free heap space

  #ifdef DEBUG_WEBSERVER
    unsigned long start = millis();
  #endif // DEBUG_WEBSERVER

    request->send_P(200, fontWoffStr, (const uint8_t*)content, content_len);

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("%s[webServer] Serving font '%s' took: %lu msec\n"),
        TimeStamp(),
        request->url().c_str(),
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // ServeFont

#if defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

// Serve a specified font from the flash file system
void ServeFontFromFile(class AsyncWebServerRequest* request, const char* path)
{
    VanBusRx.Disable();
    if (! SPIFFS.exists(path))
    {
        VanBusRx.Enable();
        return HandleNotFound(request);
    } // if

    printHttpRequest(request);

    // Skip Etag checking; browsers don't seem to use that when requesting fonts

    DeleteAllQueuedJsons();  // Maximize free heap space

  #ifdef DEBUG_WEBSERVER
    unsigned long start = millis();
  #endif // DEBUG_WEBSERVER

    request->send(SPIFFS, fontWoffStr, path);

    VanBusRx.Enable();

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("%s[webServer] Serving font '%s' from file system took: %lu msec\n"),
        TimeStamp(),
        path,
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // ServeFontFromFile

#endif // defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

// Convert the file extension to the MIME type
const char* getContentType(const String& path)
{
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".woff")) return fontWoffStr;
    if (path.endsWith(".css")) return textCssStr;
    if (path.endsWith(".js")) return textJavaScriptStr;
    if (path.endsWith(".ico")) return "image/x-icon";
    if (path.endsWith(".jpg")) return "image/jpeg";
    if (path.endsWith(".png")) return "image/png";
    return "text/plain";
} // getContentType

// Serve a specified document (text, html, css, javascript, ...) from program memory
void ServeDocument(class AsyncWebServerRequest* request, PGM_P mimeType, PGM_P content)
{
    printHttpRequest(request);

    if (request->method() != HTTP_GET) return;

  #ifdef DEBUG_WEBSERVER
    unsigned long start = millis();
  #endif // DEBUG_WEBSERVER

    bool eTagMatches = checkETag(request, md5Checksum);
    if (! eTagMatches)
    {
        DeleteAllQueuedJsons();  // Maximize free heap space

        // Serve the complete document
        AsyncWebServerResponse* response = request->beginResponse_P(200, mimeType, content);
        response->addHeader("ETag", String("\"") + md5Checksum + "\"");

        // Tells the client that it can cache the asset, but it cannot use the cached asset without
        // re-validating with the server
        response->addHeader(F("Cache-Control"), F("no-cache"));

        request->send(response);
    } // if

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("%s[webServer] %s '%s' took: %lu msec\n"),
        TimeStamp(),
        eTagMatches ? PSTR("Responding to request for") : PSTR("Serving"),
        request->url().c_str(),
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // ServeDocument

#if defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

// Serve a specified document (text, html, css, javascript, ...) from the flash file system
void ServeDocumentFromFile(class AsyncWebServerRequest* request, const char* urlPath = 0, const char* mimeType = 0)
{
    String path(urlPath == 0 ? request->url().c_str() : urlPath);

    String md5 = getMd5(path);
    if (md5.length() == 0)
    {
        // If 'getMd5(path)' returns an empty string, it means that 'path' is not found in the file system

        // Try the ".gz" file
        md5 = getMd5(path + ".gz");
        if (md5.length() == 0) return HandleNotFound(request);
    } // if

    printHttpRequest(request);

    if (request->method() != HTTP_GET) return;

  #ifdef DEBUG_WEBSERVER
    unsigned long start = millis();
  #endif // DEBUG_WEBSERVER

    bool eTagMatches = checkETag(request, md5);
    if (! eTagMatches)
    {
        DeleteAllQueuedJsons();  // Maximize free heap space

        // Get the MIME type, if necessary
        if (mimeType == 0) mimeType = getContentType(path);

        // Serve the complete document
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, path, mimeType);
        response->addHeader("ETag", String("\"") + md5 + "\"");
        response->addHeader(F("Cache-Control"), F("no-cache"));

        VanBusRx.Disable();
        request->send(response);
        VanBusRx.Enable();
    } // if

  #ifdef DEBUG_WEBSERVER
    Serial.printf_P(PSTR("%s[webServer] %s '%s' from file system took: %lu msec\n"),
        TimeStamp(),
        eTagMatches ? PSTR("Responding to request for") : PSTR("Serving"),
        request->url().c_str(),
        millis() - start);
  #endif // DEBUG_WEBSERVER
} // ServeDocumentFromFile

#endif // defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

// Serve the main HTML page
void ServeMainHtml(class AsyncWebServerRequest* request)
{
  #ifdef SERVE_MAIN_FILES_FROM_FFS

    // Serve from the flash file system
    ServeDocumentFromFile(request, "/MFD.html");

  #else

    // Serve from program memory, so updating is easy and does not need the flash file system uploader
    ServeDocument(request, PSTR("text/html"), mfd_html);

  #endif // SERVE_MAIN_FILES_FROM_FFS
} // ServeMainHtml

void SetupWebServer()
{
    // -----
    // Fonts

    webServer.on("/PeugeotNewRegular.woff", [](AsyncWebServerRequest *request)
    {
      #ifdef SERVE_FONTS_FROM_FFS
        ServeFontFromFile(request, "/PeugeotNewRegular.woff");
      #else
        ServeFont(request, PeugeotNewRegular_woff, PeugeotNewRegular_woff_len);
      #endif // SERVE_FONTS_FROM_FFS
    });
    webServer.on("/webfonts/fa-solid-900.woff", [](AsyncWebServerRequest *request)
    {
      #ifdef SERVE_FONTS_FROM_FFS
        ServeFontFromFile(request, "/fa-solid-900.woff");
      #else
        ServeFont(request, webfonts_fa_solid_900_woff, webfonts_fa_solid_900_woff_len);
      #endif // SERVE_FONTS_FROM_FFS
    });

    // -----
    // JavaScript files

    webServer.on("/jquery-3.5.1.min.js", [](AsyncWebServerRequest *request)
    {
      #ifdef SERVE_JAVASCRIPT_FROM_FFS
        ServeDocumentFromFile(request, "/jquery-3.5.1.min.js");
      #else
        ServeDocument(request, textJavaScriptStr, jQuery_js);
      #endif // SERVE_JAVASCRIPT_FROM_FFS
    });

    webServer.on("/MFD.js", [](AsyncWebServerRequest *request)
    {
      #ifdef SERVE_MAIN_FILES_FROM_FFS
        ServeDocumentFromFile(request, "/MFD.js");
      #else
        ServeDocument(request, textJavaScriptStr, mfd_js);
      #endif // SERVE_MAIN_FILES_FROM_FFS
    });

    // -----
    // Cascading style sheet files

    webServer.on("/css/all.css", [](AsyncWebServerRequest *request)
    {
      #ifdef SERVE_CSS_FROM_FFS
        ServeDocumentFromFile(request, "/all.css");
      #else
        ServeDocument(request, textCssStr, faAll_css);
      #endif // SERVE_CSS_FROM_FFS
    });

    webServer.on("/CarInfo.css", [](AsyncWebServerRequest *request)
    {
      #ifdef SERVE_CSS_FROM_FFS
        ServeDocumentFromFile(request, "/CarInfo.css");
      #else
        ServeDocument(request, textCssStr, carInfo_css);
      #endif // SERVE_CSS_FROM_FFS
    });

    // -----
    // HTML files

    webServer.on("/MFD.html", ServeMainHtml);

    // -----
    // Miscellaneous

    // Doing this will prevent the "login" popup on Android.
    webServer.on("/generate_204", HandleAndroidConnectivityCheck);
    webServer.on("/gen_204", HandleAndroidConnectivityCheck);

  #if defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

    // Try to serve any not further listed document from the flash file system
    webServer.onNotFound([](AsyncWebServerRequest *request)
    {
        ServeDocumentFromFile(request);
    });

  #else

    webServer.onNotFound(HandleNotFound);

  #endif // defined SERVE_FROM_SPIFFS || defined SERVE_FROM_LITTLEFS

    webServer.begin();
} // SetupWebServer

void LoopWebServer()
{
} // LoopWebServer
