
#include <map>
#include <limits.h>
#include <ESPAsyncWebSrv.h>  // https://github.com/dvarrel/ESPAsyncWebSrv

#ifdef PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT
#include <TimeLib.h>
#endif  // PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

// Useful Constants
#define MINS_PER_HOUR (60)

#ifdef WIFI_AP_MODE
// Defined in Wifi.ino
void WifiChangeChannel();
#endif // WIFI_AP_MODE

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

AsyncWebSocket webSocket("/ws");  // Create a web socket server on port 80

const int WEBSOCKET_INVALID_ID = 0xFF;
uint32_t websocketId = WEBSOCKET_INVALID_ID;
uint32_t websocketBackupId = WEBSOCKET_INVALID_ID;

// Maps IP address (cast to uint32_t) to last time that webSocket communication occurred on that IP address
std::map<uint32_t, unsigned long> lastWebSocketCommunication;

bool inMenu = false;  // true if user is browsing the menus
int irButtonFasterRepeat = 0;  // Some sat nav "list" screens have a slightly quicker IR repeat timing

bool IsIdConnected(uint32_t id)
{
    // Relying on short-circuit boolean evaluation
    return id != WEBSOCKET_INVALID_ID && webSocket.hasClient(id) && webSocket.client(id)->status() == WS_CONNECTED;
} // IsIdConnected

bool TryToSendJsonOnWebSocket(uint32_t id, const char* json)
{
    if (! webSocket.availableForWrite(id)) return false;

    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on


    IPAddress clientIp = webSocket.client(id)->remoteIP();

    webSocket.text(id, json);
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    lastWebSocketCommunication[clientIp] = millis();

    return true;
} // TryToSendJsonOnWebSocket

static const int N_QUEUED_JSON = 20;
int nextJsonPacketIdx = 0;
int nQueuedJsonSlotsOccupied = 0;
int maxQueuedJsonSlots = 0;
char* queuedJsonPackets[N_QUEUED_JSON];

// Save JSON data for later sending
void QueueJson(const char* json)
{
    // Only queue new data if there is enough heap space
    if (system_get_free_heap_size() < 5 * 1024)
    {
        Serial.printf_P(PSTR("==> WebSocket: not enough memory to store JSON data for later sending!\n"));
        Serial.print(F("JSON object:\n"));
        PrintJsonText(json);
        return;
    } // if

    // Free a slot if necessary
    if (queuedJsonPackets[nextJsonPacketIdx] != nullptr)
    {
        free(queuedJsonPackets[nextJsonPacketIdx]);
        queuedJsonPackets[nextJsonPacketIdx] = nullptr;
        nQueuedJsonSlotsOccupied--;
    } // if

    // Try to allocate memory
    queuedJsonPackets[nextJsonPacketIdx] = (char*) malloc(strlen(json) + 1);
    if (queuedJsonPackets[nextJsonPacketIdx] == nullptr) return;  // Return if failed to allocate

    nQueuedJsonSlotsOccupied++;
    if (nQueuedJsonSlotsOccupied > maxQueuedJsonSlots) maxQueuedJsonSlots = nQueuedJsonSlotsOccupied;

    // Copy content
    memcpy(queuedJsonPackets[nextJsonPacketIdx], json, strlen(json) + 1);

    Serial.printf_P(PSTR("==> WebSocket: storing JSON data for later sending in slot '%d'\n"), nextJsonPacketIdx);
    Serial.print(F("JSON object:\n"));
    PrintJsonText(json);

    nextJsonPacketIdx = (nextJsonPacketIdx + 1) % N_QUEUED_JSON;
} // QueueJson

// Send any JSON data that was queued
void SendQueuedJson(uint32_t id)
{
    if (id == WEBSOCKET_INVALID_ID) return;
    if (! IsIdConnected(id)) return;

    int i = nextJsonPacketIdx;
    do
    {
        if (queuedJsonPackets[i] != nullptr)
        {
            if (TryToSendJsonOnWebSocket(id, queuedJsonPackets[i]))
            {
                Serial.printf_P(
                    PSTR("%s[webSocket %lu] Sending stored %zu-byte packet no. '%d'\n"),
                    TimeStamp(),
                    id,
                    strlen(queuedJsonPackets[i]),
                    i
                );
                free(queuedJsonPackets[i]);
                queuedJsonPackets[i] = nullptr;
                nQueuedJsonSlotsOccupied--;
            } // if
        } // if

        i = (i + 1) % N_QUEUED_JSON;
    }
    while (i != nextJsonPacketIdx);
} // SendQueuedJson

// Send a (JSON) message to the WebSocket client.
// If isTestMessage is true, the message will be sent only on websocketId, not on websocketBackupId.
bool SendJsonOnWebSocket(const char* json, bool saveForLater, bool isTestMessage)
{
    if (json == 0) return true;
    if (strlen(json) <= 0) return true;

    uint32_t ids[2];
    int n = 0;
    if (IsIdConnected(websocketId))
    {
        //Serial.printf_P(PSTR("==> Will try to send on %u\n"), websocketId);
        ids[n] = websocketId;
        n++;
    } // if
    if (! isTestMessage && IsIdConnected(websocketBackupId))
    {
        //Serial.printf_P(PSTR("==> Will try to send on %u\n"), websocketBackupId);
        ids[n] = websocketBackupId;
        n++;
    } // if
    if (n == 2 && ids[0] == ids[1]) n = 1;

    if (n == 0)
    {
        if (saveForLater) QueueJson(json);

      #if DEBUG_WEBSOCKET >= 2
        // Print reason
        Serial.printf_P(
            PSTR("%s[webSocket] Unable to send %zu-byte packet: no client connected, %s\n"),
            TimeStamp(),
            strlen(json),
            saveForLater ? PSTR("stored for later") : PSTR("discarding")
        );
      #endif // DEBUG_WEBSOCKET >= 2

        return false;
    } // if

    bool result = false;
    unsigned long duration = ULONG_MAX;
    for (int i = 0; i < n; i++)
    {
        uint32_t id = ids[i];

        // First try to send anything still stored
        SendQueuedJson(id);

      #if DEBUG_WEBSOCKET >= 2
        Serial.printf_P(PSTR("%s[webSocket %lu] Sending %zu-byte packet\n"), TimeStamp(), id, strlen(json));
      #endif // DEBUG_WEBSOCKET >= 2

        unsigned long start = millis();

        if (TryToSendJsonOnWebSocket(id, json))
        {
            result = true;
            unsigned long thisDuration = millis() - start;
            if (thisDuration < duration) duration = thisDuration;
        } // if
    } // for

    if (! result
        || duration > 100)  // High possibility that this packet did in fact not come through
    {
        if (saveForLater) QueueJson(json);

        Serial.printf_P(
            PSTR("%s[webSocket] %Sailed to send %zu-byte packet%S\n"),
            TimeStamp(),
            result ? PSTR("Possibly f") : PSTR("F"),
            strlen(json),
            saveForLater ? PSTR(", stored for later sending") : result ? PSTR("") : PSTR(", discarding")
        );
    } // if

    // Print a message if the WebSocket transmission took outrageously long (normally it takes around 1-2 msec).
    // If that takes really long (seconds or more), the VAN bus Rx queue will overrun (remember, ESP8266 is
    // a single-thread system).
    if (result && duration > 100)
    {
        Serial.printf_P(
            PSTR("%s[webSocket] Sending %zu-byte packet took: %lu msec; result = %S\n"),
            TimeStamp(),
            strlen(json),
            duration,
            result ? PSTR("OK") : PSTR("FAIL")
        );
    } // if

    return result;
} // SendJsonOnWebSocket

// The client (javascript) is sending data back to the ESP
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
        // Possible values:
        // - Rate 0 (slow): menus
        // - Rate 1 (faster): sat nav lists of cities and streets
        // - Rate 2 (fastest): sat nav list of services, personal addresses and professional addresses
        irButtonFasterRepeat = clientMessage.substring(24).toInt();

      #ifdef DEBUG_IR_RECV
        Serial.printf_P(PSTR("==> irButtonFasterRepeat = %d\n"), irButtonFasterRepeat);
      #endif // DEBUG_IR_RECV
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
        int offsetMinutes = value.toInt();
        SetTimeZoneOffset(offsetMinutes);

        Serial.printf_P(
            PSTR("==> Time zone received from WebSocket client: UTC %+2d:%02d\n"),
            offsetMinutes / MINS_PER_HOUR,
            abs(offsetMinutes % MINS_PER_HOUR)
        );
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
                PSTR("==> Current date-time received from WebSocket client: %04d-%02d-%02d %02d:%02d:%02d.%03u UTC\n"),
                year(epoch), month(epoch), day(epoch), hour(epoch), minute(epoch), second(epoch), msec
            );
        } // if
    } // if

  #endif  // PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

} // ProcessWebSocketClientMessage

void WebSocketEvent(
    AsyncWebSocket* server,
    AsyncWebSocketClient* client,
    AwsEventType type,
    void* arg,
    uint8_t* data,
    size_t len)
{
    uint32_t id = client->id();

    switch(type)
    {
        case WS_EVT_DISCONNECT:
        {
            Serial.printf_P(PSTR("%s[webSocket %lu] Disconnected!\n"), TimeStamp(), id);

            if (id == websocketId)
            {
                websocketId = websocketBackupId;
                websocketBackupId = WEBSOCKET_INVALID_ID;
            }
            else if (id == websocketBackupId)
            {
                websocketBackupId = WEBSOCKET_INVALID_ID;
            } // if

          #ifdef DEBUG_WEBSOCKET
            Serial.printf_P(PSTR("==> websocketId=%u, websocketBackupId=%u\n"), websocketId, websocketBackupId);
          #endif // DEBUG_WEBSOCKET

          #if 0
          #ifdef WIFI_AP_MODE
            static int countDisconnects = 0;
            if (++countDisconnects == 3)
            {
                // Try another channel
                WifiChangeChannel();
                countDisconnects = 0;
            } // if
          #endif // WIFI_AP_MODE
          #endif // 0
        }
        break;

        case WS_EVT_CONNECT:
        {
            IPAddress clientIp = client->remoteIP();
            Serial.printf_P(PSTR("%s[webSocket %lu] Connection request from %s"),
                TimeStamp(),
                id,
                clientIp.toString().c_str());

            // Tune some TCP parameters
            client->client()->setNoDelay(true);

            if (id != websocketId)
            {
                Serial.printf_P(PSTR(" --> %S %lu\n"),
                    websocketId == WEBSOCKET_INVALID_ID ? PSTR("starting on") : PSTR("switching to"),
                    id
                );

                websocketBackupId = websocketId;
                websocketId = id;  // When sending, try first on this num
            }
            else
            {
                Serial.printf_P(PSTR(" --> ignoring (already on %lu)\n"), websocketId);
            } // if

          #ifdef DEBUG_WEBSOCKET
            Serial.printf_P(PSTR("==> websocketId=%u, websocketBackupId=%u\n"), websocketId, websocketBackupId);
          #endif // DEBUG_WEBSOCKET

            // Send ESP system data to client
            // Don't call 'SendJsonOnWebSocket' here, causes out-of-memory or stack overflow crash. Instead:
            QueueJson(EspSystemDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

            // Send Wi-Fi and IP data to client
            // Don't call 'SendJsonOnWebSocket' here, causes out-of-memory or stack overflow crash. Instead:
            QueueJson(WifiDataToJson(clientIp, jsonBuffer, JSON_BUFFER_SIZE));

            // Send equipment status data, e.g. presence of sat nav and other peripherals
            // Don't call 'SendJsonOnWebSocket' here, causes out-of-memory or stack overflow crash. Instead:
            QueueJson(EquipmentStatusDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

            // Send any JSON data that was stored for later sending
            // Don't call here, causes out-of-memory or stack overflow crash
            //SendQueuedJson(websocketId);
        }
        break;

        case WS_EVT_DATA:
        {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info != nullptr && info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
            {
                data[len] = '\0';  // TODO - not sure if this is safe

                IPAddress clientIp = client->remoteIP();
                lastWebSocketCommunication[clientIp] = millis();

                if (id != websocketId)
                {
                  #ifdef DEBUG_WEBSOCKET
                    Serial.printf_P(
                        PSTR("%s[webSocket %lu] received text: '%s' --> Switching to %lu\n"),
                        TimeStamp(), id, data, id
                    );
                  #endif // DEBUG_WEBSOCKET

                    websocketBackupId = websocketId;
                    websocketId = id;  // When sending, try first on this client id

                  #ifdef DEBUG_WEBSOCKET
                    Serial.printf_P(PSTR("==> websocketId=%u, websocketBackupId=%u\n"), websocketId, websocketBackupId);
                  #endif // DEBUG_WEBSOCKET
                }
                else
                {
                  #if DEBUG_WEBSOCKET >= 2
                    Serial.printf_P(PSTR("%s[webSocket %lu] received text: '%s'\n"), TimeStamp(), id, data);
                  #endif // DEBUG_WEBSOCKET >= 2
                } // if

                ProcessWebSocketClientMessage((char*)data);  // Process the message

                // Send any JSON data that was stored for later sending
                // Don't call here, causes out-of-memory or stack overflow crash
                //SendQueuedJson(websocketId);
            } // if
        }
        break;

        case WS_EVT_PONG:
        {
            // No further handling
        }
        break;

        case WS_EVT_ERROR:
        {
            uint16_t reasonCode = *(uint16_t*)arg;
            Serial.printf_P(PSTR("%s[webSocket %lu]: error %d occurred: '%s'\n"), TimeStamp(), id, reasonCode, data);
        }
        break;

    } // switch
} // WebSocketEvent

#ifdef WIFI_STRESS_TEST
const char* WebSocketPacketLossTestDataToJson(uint32_t packetNo, char* buf)
{
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"test\",\n"
        "\"data\":\n"
        "{\n"
            "\"packet_number\": \"%lu\""
#if WIFI_STRESS_TEST >= 2
            ",\n"
            "\"filler_00\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_01\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_02\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_03\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_04\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_05\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_06\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_07\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_08\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_09\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_0A\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_0B\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_0C\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_0D\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_0E\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_0F\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_10\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_11\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_12\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_13\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_14\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_15\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_16\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_17\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_18\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_19\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_1A\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_1B\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_1C\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_1D\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_1E\": \"1234567812345678123456781234567812345678123456781234567812345678\",\n"
            "\"filler_1F\": \"1234567812345678123456781234567812345678123456781234567812345678\""
#endif // WIFI_STRESS_TEST >= 2
            "\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, JSON_BUFFER_SIZE, jsonFormatter, packetNo);

    // JSON buffer overflow?
    if (at >= JSON_BUFFER_SIZE) return "";

    return buf;
} // WebSocketPacketLossTestDataToJson
#endif // WIFI_STRESS_TEST

void SetupWebSocket()
{
    webSocket.onEvent(WebSocketEvent);
    webServer.addHandler(&webSocket);

    Serial.print(F("WebSocket server running\n"));
} // SetupWebSocket

void LoopWebSocket()
{
    // Somehow, webSocket.cleanupClients() sometimes causes out of memory condition
    if (system_get_free_heap_size() > 2 * 1024) webSocket.cleanupClients();

  #ifdef WIFI_STRESS_TEST
    static uint32_t packetNo = 0;

    // Don't let the test frames overflow the queue
    if (IsIdConnected(websocketId) && webSocket.areAllQueuesEmpty())
    {
        bool result = SendJsonOnWebSocket(WebSocketPacketLossTestDataToJson(packetNo, jsonBuffer), false, true);
        if (result) packetNo++;
    } // if
  #endif // WIFI_STRESS_TEST

  #ifdef DEBUG_WEBSOCKET
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 5000UL)  // Arithmetic has safe roll-over
    {
        lastUpdate = millis();

        Serial.printf_P(
            PSTR("%s[webSocket] %zu clients are currently connected, websocketId=%u, websocketBackupId=%u\n"),
            TimeStamp(), webSocket.count(), websocketId, websocketBackupId
        );
    } // if
  #endif // DEBUG_WEBSOCKET
} // LoopWebSocket
