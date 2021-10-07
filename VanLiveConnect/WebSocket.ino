
// Either this for "WebSockets" (https://github.com/Links2004/arduinoWebSockets):
#include <WebSocketsServer.h>

// Or this for "WebSockets_Generic" (https://github.com/khoih-prog/WebSockets_Generic):
//#include <WebSocketsServer_Generic.h>

// Defined in PacketToJson.ino
extern const char yesStr[];
extern const char noStr[];
void PrintJsonText(const char* jsonBuffer);

// Defined in OriginalMfd.ino
extern unsigned long popupShowingSince;
extern long popupDuration;

// Defined in Esp.ino
const char* EspSystemDataToJson(char* buf, const int n);

bool inMenu = false;  // true if user is browsing the menus

// Create a web socket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

uint8_t websocketNum = 0xFF;

// Broadcast a (JSON) message to all websocket clients
void BroadcastJsonText(const char* json)
{
    if (strlen(json) <= 0) return;
    if (websocketNum == 0xFF) return;

    delay(1); // Give some time to system to process other things?

    unsigned long start = millis();

    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on
    webSocket.broadcastTXT(json);
    //webSocket.sendTXT(websocketNum, json); // Alternative
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    // Print a message if the websocket broadcast took outrageously long (normally it takes around 1-2 msec).
    // If that takes really long (seconds or more), the VAN bus Rx queue will overrun (remember, ESP8266 is
    // a single-thread system).
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

#ifdef DEBUG_WEBSOCKET
        Serial.printf_P(PSTR("[webSocket] inMenu: %S\n"), inMenu ? yesStr : noStr);
#endif // DEBUG_WEBSOCKET
    }
    else if (clientMessage.startsWith("mfd_popup_showing:"))
    {
        if (clientMessage.endsWith(":NO"))
        {
            popupShowingSince = 0;
            popupDuration = 0;

#ifdef DEBUG_WEBSOCKET
            Serial.printf_P(PSTR("[webSocket] isPopupShowing: NO\n"));
#endif // DEBUG_WEBSOCKET
        }
        else
        {
            popupShowingSince = millis();
            popupDuration = clientMessage.substring(18).toInt();

#ifdef DEBUG_WEBSOCKET
            Serial.printf_P(PSTR("[webSocket] isPopupShowing: YES (%ld msec)\n"), popupDuration);
#endif // DEBUG_WEBSOCKET
        } // if
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
            BroadcastJsonText(EspSystemDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

            // Send Wi-Fi and IP data to client
            BroadcastJsonText(WifiDataToJson(clientIp, jsonBuffer, JSON_BUFFER_SIZE));

            // Send equipment status data, e.g. presence of sat nav and other devices
            BroadcastJsonText(EquipmentStatusDataToJson(jsonBuffer, JSON_BUFFER_SIZE));
        }
        break;

        case WStype_TEXT:
        {
#ifdef DEBUG_WEBSOCKET
            Serial.printf_P(PSTR("[webSocket %u] received text: %s\n"), num, payload);
#endif // DEBUG_WEBSOCKET

            // Process the message
            ProcessWebSocketClientMessage((char*)payload);

            // Parse reports back from the browser (client) side
/*
            if (strncmp((char*)payload, "in_menu:", 8) == 0)
            {
                inMenu = strncmp((char*)payload + 8, "NO", 2) != 0;
            }
            else if (strncmp((char*)payload, "mfd_popup_showing:", 18) == 0)
            {
                popupShowingSince = strncmp((char*)payload + 18, "NO", 2) != 0 ? millis() : 0;
            } // if
*/
/*
            String message((char*)payload);

            Serial.println(message);  // TODO - remove

            if (message.startsWith("in_menu:"))
            {
                inMenu = message.endsWith(":YES");
            }
            else if (message.startsWith("mfd_popup_showing:"))
            {
                if (message.endsWith(":NO"))
                {
                    popupShowingSince = 0;
                    popupDuration = 0;
                }
                else
                {
// TODO - crashes ESP
//                    popupShowingSince = millis();
//                    popupDuration = message.substring(18).toInt();
                } // if
            } // if
*/
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
