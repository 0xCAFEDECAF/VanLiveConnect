
// Either this for "WebSockets" (https://github.com/Links2004/arduinoWebSockets):
//
#include <WebSocketsServer.h>

// Or this for "WebSockets_Generic" (https://github.com/khoih-prog/WebSockets_Generic):
//#include <WebSocketsServer_Generic.h>

// In the file:
//    ...\Documents\Arduino\libraries\WebSockets\src\WebSockets.h
// the line:
//   #define WEBSOCKETS_TCP_TIMEOUT (5000)
// must be changed to:
//   #define WEBSOCKETS_TCP_TIMEOUT (1000)
// to prevent the VAN bus receiver from overrunning when the web socket disconnects
#if WEBSOCKETS_TCP_TIMEOUT > 1000
#error "...\Arduino\libraries\WebSockets\src\WebSockets.h:"
#error "Value for '#define WEBSOCKETS_TCP_TIMEOUT' is too large; set to at most 1000!"
#endif

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

WebSocketsServer webSocket = WebSocketsServer(81);  // Create a web socket server on port 81
uint8_t prevWebsocketNum = 0xFF;
uint8_t websocketNum = 0xFF;
bool inMenu = false;  // true if user is browsing the menus

// Send a (JSON) message to the websocket client
void SendJsonOnWebSocket(const char* json)
{
    if (strlen(json) <= 0) return;
    if (websocketNum == 0xFF) return;

    delay(1); // Give some time to system to process other things?

    unsigned long start = millis();

    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on

    //webSocket.broadcastTXT(json);
    // No, serve only the last one connected (the others are probably already dead)
    webSocket.sendTXT(websocketNum, json);

    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    // Print a message if the websocket transmission took outrageously long (normally it takes around 1-2 msec).
    // If that takes really long (seconds or more), the VAN bus Rx queue will overrun (remember, ESP8266 is
    // a single-thread system).
    unsigned long duration = millis() - start;
    if (duration > 100)
    {
        Serial.printf_P(
            PSTR("Sending %zu JSON bytes via 'webSocket.sendTXT' took: %lu msec\n"),
            strlen(json),
            duration);

        Serial.print(F("JSON object:\n"));
        PrintJsonText(json);
    } // if
} // SendJsonOnWebSocket

void ProcessWebSocketClientMessage(const char* payload)
{
    String clientMessage(payload);

    if (clientMessage == "") return;

    if (clientMessage.startsWith("in_menu:"))
    {
        inMenu = clientMessage.endsWith(":YES");
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
    } // if
} // ProcessWebSocketClientMessage

void WebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length)
{
    switch(type)
    {
        case WStype_DISCONNECTED:
        {
            Serial.printf("[webSocket %u] Disconnected!\n", num);
            if (num == websocketNum)
            {
                websocketNum = prevWebsocketNum;
                prevWebsocketNum = 0xFF;
            } // if

            if (num == prevWebsocketNum)
            {
                prevWebsocketNum = 0xFF;
            } // if
        }
        break;

        case WStype_CONNECTED:
        {
            IPAddress clientIp = webSocket.remoteIP(num);
            Serial.printf_P(PSTR("[webSocket %u] Connected from %d.%d.%d.%d url: %s\n"),
                num,
                clientIp[0], clientIp[1], clientIp[2], clientIp[3],
                payload);

            if (num != websocketNum)
            {
                prevWebsocketNum = websocketNum;
                websocketNum = num;
            } // if

            // Send ESP system data to client
            SendJsonOnWebSocket(EspSystemDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

            // Send Wi-Fi and IP data to client
            SendJsonOnWebSocket(WifiDataToJson(clientIp, jsonBuffer, JSON_BUFFER_SIZE));

            // Send equipment status data, e.g. presence of sat nav and other devices
            SendJsonOnWebSocket(EquipmentStatusDataToJson(jsonBuffer, JSON_BUFFER_SIZE));
        }
        break;

        case WStype_TEXT:
        {
          #ifdef DEBUG_WEBSOCKET
            Serial.printf("[webSocket %u] received text: '%s'\n", num, payload);
          #endif // DEBUG_WEBSOCKET

            if (num != websocketNum)
            {
                prevWebsocketNum = websocketNum;
                websocketNum = num;
            } // if

            ProcessWebSocketClientMessage((char*)payload);  // Process the message
        }
        break;
    } // switch
} // WebSocketEvent

void SetupWebSocket()
{
    webSocket.begin();
    webSocket.onEvent(WebSocketEvent);
} // SetupWebSocket

void LoopWebSocket()
{
    webSocket.loop();
} // LoopWebSocket
