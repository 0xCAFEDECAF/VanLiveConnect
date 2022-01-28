
// Either this for "WebSockets" (https://github.com/Links2004/arduinoWebSockets):
#include <WebSocketsServer.h>

// Or this for "WebSockets_Generic" (https://github.com/khoih-prog/WebSockets_Generic):
//#include <WebSocketsServer_Generic.h>

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

bool inMenu = false;  // true if user is browsing the menus

// Create a web socket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

uint8_t websocketNum = 0xFF;

// Send a (JSON) message to the websocket client
void SendJsonText(const char* json)
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
} // SendJsonText

void ProcessWebSocketClientMessage(const char* payload)
{
    String clientMessage(payload);

    if (clientMessage == "") return;

  #ifdef DEBUG_WEBSOCKET
    Serial.println("[webSocket] received text: '" + clientMessage + "'");
  #endif // DEBUG_WEBSOCKET

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
    }
    else if (clientMessage.startsWith("mfd_distance_unit:"))
    {
        String value = clientMessage.substring(18);

        mfdDistanceUnit =
            value == "set_units_mph" ? MFD_DISTANCE_UNIT_IMPERIAL :
            MFD_DISTANCE_UNIT_METRIC;

        ResetPacketPrevData();
    }
    else if (clientMessage.startsWith("mfd_temperature_unit:"))
    {
        String value = clientMessage.substring(21);

        mfdTemperatureUnit =
            value == "set_units_deg_fahrenheit" ? MFD_TEMPERATURE_UNIT_FAHRENHEIT :
            MFD_TEMPERATURE_UNIT_CELSIUS;

        ResetPacketPrevData();
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
            websocketNum = 0xFF;
        }
        break;

        case WStype_CONNECTED:
        {
            IPAddress clientIp = webSocket.remoteIP(num);
            Serial.printf_P(PSTR("[webSocket %u] Connected from %d.%d.%d.%d url: %s\n"),
                num,
                clientIp[0], clientIp[1], clientIp[2], clientIp[3],
                payload);

            websocketNum = num;

            // Send ESP system data to client
            SendJsonText(EspSystemDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

            // Send Wi-Fi and IP data to client
            SendJsonText(WifiDataToJson(clientIp, jsonBuffer, JSON_BUFFER_SIZE));

            // Send equipment status data, e.g. presence of sat nav and other devices
            SendJsonText(EquipmentStatusDataToJson(jsonBuffer, JSON_BUFFER_SIZE));
        }
        break;

        case WStype_TEXT:
        {
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
