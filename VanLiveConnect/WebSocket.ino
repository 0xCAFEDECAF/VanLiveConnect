
// Using "WebSockets" library (https://github.com/Links2004/arduinoWebSockets):
#include <WebSocketsServer.h>

#ifdef STRICT_COMPILATION
  #if WEBSOCKETS_NETWORK_TYPE != NETWORK_ESP8266_ASYNC

  // Note: setting #define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP8266_ASYNC in WebSockets.h
  // gives not only more reliable performance of the WebSocket, but also leads to less CRC error
  // packets from the VAN bus.

  #warning "Please compile the WebSockets library for TCP asynchronous mode! To do this, define"
  #warning "WEBSOCKETS_NETWORK_TYPE to be NETWORK_ESP8266_ASYNC in the header file"
  #warning "...\Arduino\libraries\WebSockets\src\WebSockets.h, around line 118."

  // Note: if not using TCP asynchronous mode, the following #define must be changed from:
  //   #define WEBSOCKETS_TCP_TIMEOUT (5000)
  // to:
  //   #define WEBSOCKETS_TCP_TIMEOUT (1000)
  // as defined in the file
  //    ...\Documents\Arduino\libraries\WebSockets\src\WebSockets.h, around line 100
  // This is to prevent the VAN bus receiver queue from overrunning when the web socket disconnects
  //
  // #if WEBSOCKETS_TCP_TIMEOUT > 1000
  // #error "...\Arduino\libraries\WebSockets\src\WebSockets.h:"
  // #error "Value for '#define WEBSOCKETS_TCP_TIMEOUT' is too large; advised to set to 1000!"
  // #endif

  #endif  // WEBSOCKETS_NETWORK_TYPE != NETWORK_ESP8266_ASYNC
#endif  // STRICT_COMPILATION

#ifdef PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT
#include <TimeLib.h>
#endif  // PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

// Defined in PacketToJson.ino
extern uint8_t mfdDistanceUnit;
extern uint8_t mfdTemperatureUnit;
extern uint8_t mfdTimeUnit;
extern const char yesStr[];
extern const char noStr[];
void PrintJsonText(const char* jsonBuffer);
void ResetPacketPrevData();

// Defined in OriginalMfd.ino
extern uint8_t mfdLanguage;
void NoPopup();
void NotificationPopupShowing(unsigned long since, long duration);

// Defined in Esp.ino
const char* EspSystemDataToJson(char* buf, const int n);

// Defined in DateTime.ino
void SetTimeZoneOffset(int newTimeZoneOffset);
bool SetTime(uint32_t epoch, uint32_t msec);

WebSocketsServer webSocket = WebSocketsServer(81);  // Create a web socket server on port 81

#define WEBSOCKET_INVALID_NUM (0xFF)
uint8_t prevWebsocketNum = WEBSOCKET_INVALID_NUM;
uint8_t websocketNum = WEBSOCKET_INVALID_NUM;
bool inMenu = false;  // true if user is browsing the menus
bool irButtonFasterRepeat = false;  // Some sat nav "list" screens have a slightly quicker IR repeat timing

#define N_SAVED_JSON 3
int savedJsonIdx = 0;
char* savedJson[N_SAVED_JSON];

// Save JSON data for later sending
void SaveJsonForLater(const char* json)
{
    // Free a slot if necessary
    if (savedJson[savedJsonIdx] != NULL) free(savedJson[savedJsonIdx]);

    // Allocate memory
    savedJson[savedJsonIdx] = (char*) malloc(strlen(json) + 1);
    if (savedJson[savedJsonIdx] == NULL) return;

    // Copy content, for sending later
    memcpy(savedJson[savedJsonIdx], json, strlen(json) + 1);

    Serial.printf_P(PSTR("==> WebSocket: saved JSON data for later sending in slot '%d'\n"), savedJsonIdx);
    Serial.print(F("JSON object:\n"));
    PrintJsonText(json);

    savedJsonIdx = (savedJsonIdx + 1) % N_SAVED_JSON;
} // SaveJsonForLater

// Send any JSON data that was saved for later sending
void SendSavedJson()
{
    int i = savedJsonIdx;
    do
    {
        if (savedJson[i] != NULL)
        {
            Serial.printf_P(PSTR("==> WebSocket: sending saved JSON data packet no. '%d'\n"), i);
            SendJsonOnWebSocket(savedJson[i]);
            free(savedJson[i]);
            savedJson[i] = NULL;
        } // if
        i = (i + 1) % N_SAVED_JSON;
    }
    while (i != savedJsonIdx);
} // SendSavedJson

void FreeSavedJson()
{
    int i = savedJsonIdx;
    do
    {
        if (savedJson[i] != NULL)
        {
            free(savedJson[i]);
            savedJson[i] = NULL;
        } // if
        i = (i + 1) % N_SAVED_JSON;
    }
    while (i != savedJsonIdx);
} // FreeSavedJson

// Send a (JSON) message to the WebSocket client
void SendJsonOnWebSocket(const char* json, bool savePacketForLater)
{
    if (json == 0) return;
    if (strlen(json) <= 0) return;
    if (websocketNum == WEBSOCKET_INVALID_NUM)
    {
        if (! savePacketForLater) return;

        SaveJsonForLater(json);
        return;
    } // if

    bool result = false;
    unsigned long start = millis();

    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on

    for (int attempt = 0; attempt < 3; attempt++)
    {
        if (attempt != 0)
        {
            Serial.printf_P(
                PSTR("Sending %zu JSON bytes via 'webSocket.sendTXT' attempt no.: %d\n"),
                strlen(json),
                attempt + 1
            );
        } // if

        // Don't broadcast (webSocket.broadcastTXT(json)); serve only the last one connected
        // (the others are probably already dead)
        result = webSocket.sendTXT(websocketNum, json);
        if (result) break;
    } // for

    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    unsigned long duration = millis() - start;

    if (! result) Serial.printf_P(PSTR("FAILED to sending %zu JSON bytes via 'webSocket.sendTXT'\n"), strlen(json));

    if (! result || duration > 100)
    {
        // High possibility that this packet did in fact not come through
        if (savePacketForLater) SaveJsonForLater(json);
    }
    else
    {
        SendSavedJson();
    } // if

    // Print a message if the WebSocket transmission took outrageously long (normally it takes around 1-2 msec).
    // If that takes really long (seconds or more), the VAN bus Rx queue will overrun (remember, ESP8266 is
    // a single-thread system).
    if (duration > 100)
    {
        Serial.printf_P(
            PSTR("Sending %zu JSON bytes via 'webSocket.sendTXT' took: %lu msec; result = %S\n"),
            strlen(json),
            duration,
            result ? PSTR("OK") : PSTR("FAIL"));

        Serial.print(F("JSON object:\n"));
        PrintJsonText(json);
    } // if
} // SendJsonOnWebSocket

// The client (javascript) is sending data to the ESP
void ProcessWebSocketClientMessage(const char* payload)
{
    String clientMessage(payload);

    if (clientMessage == "") return;

    if (clientMessage.startsWith("in_menu:"))
    {
        inMenu = clientMessage.endsWith(":YES");
    }
    else if (clientMessage.startsWith("ir_button_faster_repeat:"))
    {
        irButtonFasterRepeat = clientMessage.endsWith(":YES");
    }
    else if (clientMessage.startsWith("mfd_popup_showing:"))
    {
        // We need to know when a popup is showing, e.g. to know when to ignore a "MOD" button press from the
        // IR remote control.
        if (clientMessage.endsWith(":NO")) NoPopup();
        else NotificationPopupShowing(millis(), clientMessage.substring(18).toInt());
    }

    // TODO - keep this?
    else if (clientMessage.startsWith("mfd_language:"))
    {
        String value = clientMessage.substring(13);

        mfdLanguage =
            value == "set_language_french" ? MFD_LANGUAGE_FRENCH :
            value == "set_language_german" ? MFD_LANGUAGE_GERMAN :
            value == "set_language_spanish" ? MFD_LANGUAGE_SPANISH :
            value == "set_language_italian" ? MFD_LANGUAGE_ITALIAN :
            value == "set_language_dutch" ? MFD_LANGUAGE_DUTCH :
            MFD_LANGUAGE_ENGLISH;

        decimalSeparatorChar =
            mfdLanguage == MFD_LANGUAGE_FRENCH ? ',' :
            mfdLanguage == MFD_LANGUAGE_GERMAN ? ',' :
            mfdLanguage == MFD_LANGUAGE_SPANISH ? ',' :
            mfdLanguage == MFD_LANGUAGE_ITALIAN ? ',' :
            mfdLanguage == MFD_LANGUAGE_DUTCH ? ',' :
            '.';
    }
    else if (clientMessage.startsWith("mfd_distance_unit:"))
    {
        String value = clientMessage.substring(18);

        uint8_t prevMfdDistanceUnit = mfdDistanceUnit;
        mfdDistanceUnit =
            value == "set_units_mph" ? MFD_DISTANCE_UNIT_IMPERIAL :
            MFD_DISTANCE_UNIT_METRIC;

        if (mfdDistanceUnit != prevMfdDistanceUnit) ResetPacketPrevData();
    }
    else if (clientMessage.startsWith("mfd_temperature_unit:"))
    {
        String value = clientMessage.substring(21);

        uint8_t prevMfdTemperatureUnit = mfdTemperatureUnit;
        mfdTemperatureUnit =
            value == "set_units_deg_fahrenheit" ? MFD_TEMPERATURE_UNIT_FAHRENHEIT :
            MFD_TEMPERATURE_UNIT_CELSIUS;

        if (mfdTemperatureUnit != prevMfdTemperatureUnit) ResetPacketPrevData();
    }
    else if (clientMessage.startsWith("mfd_time_unit:"))
    {
        String value = clientMessage.substring(14);

        mfdTimeUnit =
            value == "set_units_12h" ? MFD_TIME_UNIT_12H :
            MFD_TIME_UNIT_24H;
    }

  #ifdef PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

    else if (clientMessage.startsWith("time_offset:"))
    {
        String value = clientMessage.substring(12);
        SetTimeZoneOffset(value.toInt());
    }
    else if (clientMessage.startsWith("date_time:"))
    {
        String value = clientMessage.substring(10);
        uint64_t epoch_msec = atoll(value.c_str());

        uint32_t epoch = epoch_msec / 1000ULL;
        uint32_t msec = epoch_msec % 1000ULL;

        // Note: no idea how long this message has been in transport, so the time set here may be
        // off a few (hundred) milliseconds...
        if (SetTime(epoch, msec))
        {
            Serial.printf_P(
                PSTR("==> Current date-time received from WebSocket client: %02d-%02d-%04d %02d:%02d:%02d.%03u UTC\n"),
                day(epoch), month(epoch), year(epoch), hour(epoch), minute(epoch), second(epoch), msec
            );
        } // if
    } // if

  #endif  // PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

} // ProcessWebSocketClientMessage

void WebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length)
{
    switch(type)
    {
        case WStype_DISCONNECTED:
        {
            Serial.printf_P(PSTR("%s[webSocket %u] Disconnected!\n"), TimeStamp(), num);
            if (num == websocketNum)
            {
                websocketNum = prevWebsocketNum;
                prevWebsocketNum = WEBSOCKET_INVALID_NUM;
            } // if

            if (num == prevWebsocketNum)
            {
                prevWebsocketNum = WEBSOCKET_INVALID_NUM;
            } // if
        }
        break;

        case WStype_CONNECTED:
        {
            IPAddress clientIp = webSocket.remoteIP(num);
            Serial.printf_P(PSTR("%s[webSocket %u] Connected from %d.%d.%d.%d url: %s\n"),
                TimeStamp(),
                num,
                clientIp[0], clientIp[1], clientIp[2], clientIp[3],
                payload);

            if (num != websocketNum)
            {
                // Serve only the last one connected
                prevWebsocketNum = websocketNum;
                websocketNum = num;
            } // if

            // Send ESP system data to client
            SendJsonOnWebSocket(EspSystemDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

            // Send Wi-Fi and IP data to client
            SendJsonOnWebSocket(WifiDataToJson(clientIp, jsonBuffer, JSON_BUFFER_SIZE));

            // Send equipment status data, e.g. presence of sat nav and other peripherals
            SendJsonOnWebSocket(EquipmentStatusDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

            // Send any JSON data that was saved for later sending
            SendSavedJson();
        }
        break;

        case WStype_TEXT:
        {
          #ifdef DEBUG_WEBSOCKET
            Serial.printf_P(PSTR("%s[webSocket %u] received text: '%s'"), TimeStamp(), num, payload);
          #endif // DEBUG_WEBSOCKET

            if (num != websocketNum)
            {
                // If we were not serving any, switch to this one
                if (websocketNum == WEBSOCKET_INVALID_NUM)
                {
                    prevWebsocketNum = websocketNum;
                    websocketNum = num;
                }
                else
                {
                    // Just keep on serving the last one that connected

                  #ifdef DEBUG_WEBSOCKET
                    Serial.printf_P(PSTR(" --> ignoring (listening only to webSocket %u)\n"), websocketNum);
                  #endif // DEBUG_WEBSOCKET

                    break;
                } // if
            } // if

          #ifdef DEBUG_WEBSOCKET
            Serial.println();
          #endif // DEBUG_WEBSOCKET

            ProcessWebSocketClientMessage((char*)payload);  // Process the message
        }
        break;

      # if 0
        case WStype_PING:
        {
            // pong will be sent automatically
            Serial.printf("[webSocket %u] get ping\n", num);
        }
        break;

        case WStype_PONG:
        {
            // answer to a ping we send
            Serial.printf("[webSocket %u] get pong\n", num);
        }
        break;
      #endif

    } // switch
} // WebSocketEvent

void SetupWebSocket()
{
    webSocket.begin();
    webSocket.onEvent(WebSocketEvent);

    // TODO - keep this? if WEBSOCKETS_TCP_TIMEOUT is set to 500, seems to disconnect a lot
    // Note: no heartbeat packets are sent when using WEBSOCKETS_NETWORK_TYPE NETWORK_ESP8266_ASYNC
    // (as '#define'd in WebSockets.h)
    webSocket.enableHeartbeat(200, 5000, 0);

    Serial.printf_P(PSTR("WebSocket server running in \"%S\" mode; timeout value = %d msec\n"),
      #if WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC
        PSTR("Async TCP"),
      #else
        PSTR("normal (synchronous) TCP"),
      #endif  // WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC
        WEBSOCKETS_TCP_TIMEOUT
    );
} // SetupWebSocket

void LoopWebSocket()
{
  // Async interface does not need a loop call
  #if(WEBSOCKETS_NETWORK_TYPE != NETWORK_ESP8266_ASYNC)
    webSocket.loop();
  #endif  // WEBSOCKETS_NETWORK_TYPE != NETWORK_ESP8266_ASYNC
} // LoopWebSocket
