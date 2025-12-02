
#include <map>
#include <limits.h>

#ifdef PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT
#include <TimeLib.h>
#endif  // PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

// Useful Constants
#define MINS_PER_HOUR (60)

// Defined in PacketToJson.ino
extern uint8_t mfdDistanceUnit;
extern uint8_t mfdTemperatureUnit;
extern uint8_t mfdTimeUnit;
extern int16_t satnavServiceListSize;
void PrintJsonText(const char* jsonBuffer);
void ResetPacketPrevData();

// Defined in OriginalMfd.ino
extern uint8_t mfdLanguage;
void NoPopup();
void NotificationPopupShowing(unsigned long since, unsigned long duration);

// Defined in Esp.ino
const char* EspSystemDataToJson(char* buf, const int n);

// Defined in DateTime.ino
void SetTimeZoneOffset(int newTimeZoneOffset);
bool SetTime(uint32_t epoch, uint32_t msec);

AsyncWebSocket webSocket("/ws");  // Create a web socket server on port 80

const int WEBSOCKET_INVALID_ID = 0xFF;
uint32_t websocketId_1 = WEBSOCKET_INVALID_ID;
uint32_t websocketId_2 = WEBSOCKET_INVALID_ID;

// Maps IP address (cast to uint32_t) to last time that webSocket communication occurred on that IP address
std::map<uint32_t, unsigned long> lastWebSocketCommunication;

// Counts the number of new connection requests from the WebSocket client
int nWebSocketConnections = 0;

// Data coming in from the WebSocket client (user)
bool inMenu = false;  // true if user is browsing the menus
bool satnavDisclaimerAccepted = false;  // true if user has accepted the sat nav disclaimer screen
int irButtonFasterRepeat = 0;  // Some sat nav "list" screens have a slightly quicker IR repeat timing

// Check if a webSocket on a specific ID is connected
bool IsIdConnected(uint32_t id)
{
    // Relying on short-circuit boolean evaluation
    return id != WEBSOCKET_INVALID_ID && webSocket.hasClient(id) && webSocket.client(id)->status() == WS_CONNECTED;
} // IsIdConnected

// Try to send a data packet on a specific webSocket. Fails if the webSocket is not connected or its queue is full
// (not available for writing).
bool TryToSendJsonOnWebSocket(uint32_t id, const char* json)
{
  #if DEBUG_WEBSOCKET >= 3
    Serial.printf_P(PSTR("%s[webSocket %lu] Trying to send %zu-byte packet\n"), TimeStamp(), id, strlen(json));
  #endif // DEBUG_WEBSOCKET >= 3

    if (! IsIdConnected(id)) return false;

    // Don't let any message overflow the queue. Messages written into a full queue are discarded by the
    // AsyncWebSocket class
    if (! webSocket.availableForWrite(id)) return false;

    webSocket.text(id, json);

    IPAddress clientIp = webSocket.client(id)->remoteIP();
    lastWebSocketCommunication[clientIp] = millis();

    return true;
} // TryToSendJsonOnWebSocket

static const int N_QUEUED_JSON = 30;
int nextJsonPacketIdx = 0;
int nQueuedJsonSlotsOccupied = 0;
int maxQueuedJsonSlots = 0;

struct JsonPacket_t
{
    char* packet;
    uint32_t toBeSentOnId;
    unsigned long lastSent;
    uint32_t lastSentOnId_1;
    uint32_t lastSentOnId_2;
}; // JsonPacket_t

JsonPacket_t queuedJsonPackets[N_QUEUED_JSON];

int countQueuedJsons()
{
    int result = 0;
    int i = nextJsonPacketIdx;
    do
    {
        JsonPacket_t* entry = queuedJsonPackets + i;
        if (entry->packet != nullptr) result++;
        if (++i == N_QUEUED_JSON) i = 0;
    }
    while (i != nextJsonPacketIdx);
    return result;
} // countQueuedJsons

void FreeQueuedJson(JsonPacket_t* entry)
{
    if (entry->packet == nullptr) return;

    free(entry->packet);
    entry->packet = nullptr;
    entry->lastSent = 0;
    entry->lastSentOnId_1 = 0;
    entry->lastSentOnId_2 = 0;
    nQueuedJsonSlotsOccupied--;
} // FreeQueuedJson

void DeleteAllQueuedJsons()
{
    int i = nextJsonPacketIdx;
    do
    {
        JsonPacket_t* entry = queuedJsonPackets + i;
        FreeQueuedJson(entry);
        if (++i == N_QUEUED_JSON) i = 0;
    }
    while (i != nextJsonPacketIdx);
} // DeleteAllQueuedJsons

// Save a JSON data packet for later sending.
// Optionally, pass one or two WebSocket IDs: the packet will not be re-sent to these IDs.
void QueueJson(const char* json, uint32_t lastSentOnId_1 = 0, uint32_t lastSentOnId_2 = 0)
{
  #if DEBUG_WEBSOCKET >= 2
    int currentJsonPacketIdx;
  #endif // DEBUG_WEBSOCKET >= 2
    JsonPacket_t* entry;

    do
    {
        entry = queuedJsonPackets + nextJsonPacketIdx;

        // Free a slot if necessary
        FreeQueuedJson(entry);

      #if DEBUG_WEBSOCKET >= 2
        currentJsonPacketIdx = nextJsonPacketIdx;
      #endif // DEBUG_WEBSOCKET >= 2

        if (++nextJsonPacketIdx == N_QUEUED_JSON) nextJsonPacketIdx = 0;

    } while (system_get_free_heap_size() < 20 * 1024 && countQueuedJsons() > 0);

    // Try to allocate memory
    entry->packet = (char*) malloc(strlen(json) + 1);
    if (entry->packet == nullptr) return;  // Return if failed to allocate

    nQueuedJsonSlotsOccupied++;
    if (nQueuedJsonSlotsOccupied > maxQueuedJsonSlots) maxQueuedJsonSlots = nQueuedJsonSlotsOccupied;

    // Copy content and meta-data
    memcpy(entry->packet, json, strlen(json) + 1);
    entry->lastSentOnId_1 = lastSentOnId_1;
    entry->lastSentOnId_2 = lastSentOnId_2;
    if (lastSentOnId_1 != 0 || lastSentOnId_2 != 0) entry->lastSent = millis();

  #if DEBUG_WEBSOCKET >= 2
    Serial.printf_P(
        PSTR("%s[webSocket] %s %zu-byte packet for %ssending in slot '%d'\n"),
        TimeStamp(),
        lastSentOnId_1 == 0 && lastSentOnId_2 == 0 ? PSTR("Saving") : PSTR("Keeping"),
        strlen(json),
        lastSentOnId_1 == 0 && lastSentOnId_2 == 0 ? PSTR("later ") : PSTR("re-"),
        currentJsonPacketIdx
    );
  #endif // DEBUG_WEBSOCKET >= 2

} // QueueJson

static unsigned long lastSendQueued = 0;

// Send any queued JSON packets to a specific WebSocket ID
void SendQueuedJson(uint32_t id)
{
    if (! IsIdConnected(id)) return;

    int i = nextJsonPacketIdx;
    do
    {
        JsonPacket_t* entry = queuedJsonPackets + i;

        if (entry->packet == nullptr) goto NEXT;

        // Don't resend a queued packet on the same WebSocket ID
        if (entry->lastSentOnId_1 == id || entry->lastSentOnId_2 == id) goto NEXT;

        lastSendQueued = millis();

        if (! TryToSendJsonOnWebSocket(id, entry->packet)) goto NEXT;

      #ifdef DEBUG_WEBSOCKET
        Serial.printf_P(
            PSTR("%s[webSocket %lu] Sent stored %zu-byte packet no. '%d'\n"),
            TimeStamp(),
            id,
            strlen(entry->packet),
            i
        );
      #endif // DEBUG_WEBSOCKET

        // Don't reset the age
        if (entry->lastSent == 0) entry->lastSent = millis();

        if (entry->lastSentOnId_1 == 0) entry->lastSentOnId_1 = id;
        else if (entry->lastSentOnId_2 == 0) entry->lastSentOnId_2 = id;
        else if (entry->lastSentOnId_1 != id && entry->lastSentOnId_2 != id) entry->lastSentOnId_1 = id;

      NEXT:
        if (++i == N_QUEUED_JSON) i = 0;
    }
    while (i != nextJsonPacketIdx);
} // SendQueuedJson

// Clean up old saved JSON data that was queued.
// Optionally, pass a WebSocket ID: JSON packets that were already sent to that ID will be cleaned up immediately.
void CleanupQueuedJsons(uint32_t id = 0)
{
    int i = nextJsonPacketIdx;
    do
    {
        JsonPacket_t* entry = queuedJsonPackets + i;
        unsigned long age = 0;

        if (entry->packet == nullptr) goto NEXT;
        if (entry->lastSentOnId_1 == 0 && entry->lastSentOnId_2 == 0) goto NEXT;  // Keep packets that were not yet sent

        age = millis() - entry->lastSent;  // Arithmetic has safe roll-over

        if (age >= 10000UL  // Clean up old packets after 10 seconds
            ||
            (id != 0 // If an id was passed, clean up all packets already sent on that id
             &&
             (entry->lastSentOnId_1 == id || entry->lastSentOnId_2 == id)
            )
           )
        {
          #if DEBUG_WEBSOCKET >= 3
            Serial.printf_P(
                PSTR("%s[webSocket] Cleaning up %zu-byte packet no. '%d', age=%lu, lastSentOn=%lu,%lu\n"),
                TimeStamp(),
                strlen(entry->packet),
                i,
                age,
                entry->lastSentOnId_1,
                entry->lastSentOnId_2
            );
          #endif // DEBUG_WEBSOCKET >= 3

            FreeQueuedJson(entry);
        } // if

      NEXT:
        if (++i == N_QUEUED_JSON) i = 0;
    }
    while (i != nextJsonPacketIdx);
} // CleanupQueuedJsons

// Send a (JSON) message to the WebSocket client.
// If isTestMessage is true, the message will be sent only on websocketId_1, not on websocketId_2.
bool SendJsonOnWebSocket(const char* json, bool saveForLater, bool isTestMessage)
{
    if (json == 0) return true;
    if (strlen(json) <= 0) return true;

    uint32_t ids[2];
    int n = 0;
    if (IsIdConnected(websocketId_1))
    {
        ids[n] = websocketId_1;
        n++;
    } // if
    if (! isTestMessage && IsIdConnected(websocketId_2))
    {
        ids[n] = websocketId_2;
        n++;
    } // if
    if (n == 2 && ids[0] == ids[1]) n = 1;

    if (n == 0)
    {
      #if DEBUG_WEBSOCKET >= 2
        // Print reason
        Serial.printf_P(
            PSTR("%s[webSocket] Unable to send %zu-byte packet: no client connected, %s\n"),
            TimeStamp(),
            strlen(json),
            saveForLater ? PSTR("stored for later") : PSTR("discarding")
        );
      #endif // DEBUG_WEBSOCKET >= 2

        if (saveForLater) QueueJson(json);

        return false;
    } // if

    bool result = false;
    unsigned long duration = ULONG_MAX;
    uint32_t lastSentOnId_1 = 0;
    uint32_t lastSentOnId_2 = 0;
    for (int i = 0; i < n; i++)
    {
        uint32_t id = ids[i];

        // First try to send anything still stored
        SendQueuedJson(id);

        unsigned long start = millis();

        if (TryToSendJsonOnWebSocket(id, json))
        {
            unsigned long thisDuration = millis() - start;
            if (thisDuration < duration) duration = thisDuration;

          #if DEBUG_WEBSOCKET >= 2
            Serial.printf_P(
                PSTR("%s[webSocket %lu] Sent %zu-byte packet\n"),
                TimeStamp(),
                id,
                strlen(json)
            );
          #endif // DEBUG_WEBSOCKET >= 2

            result = true;
            if (lastSentOnId_1 == 0) lastSentOnId_1 = id; else lastSentOnId_2 = id;
        } // if
    } // for

    if (saveForLater) QueueJson(json, lastSentOnId_1, lastSentOnId_2);

    if (! result)
    {
      #ifdef DEBUG_WEBSOCKET
        Serial.printf_P(
            PSTR("%s[webSocket] Failed to send %zu-byte packet%s\n"),
            TimeStamp(),
            strlen(json),
            saveForLater ? PSTR(", stored for later sending") : PSTR(", discarding")
        );

      #if DEBUG_WEBSOCKET >= 2
        Serial.print(F("JSON object:\n"));
        PrintJsonText(json);
      #endif // DEBUG_WEBSOCKET >= 2

      #endif // DEBUG_WEBSOCKET
    }
    else
    {
        // Print a message if the WebSocket transmission took outrageously long (normally it takes around 1-2 msec).
        // If that takes really long (seconds or more), the VAN bus Rx queue will overrun (remember, ESP8266 is
        // a single-thread system).
        if (duration > 100)
        {
            Serial.printf_P(
                PSTR("%s[webSocket] Sending %zu-byte packet took: %lu msec\n"),
                TimeStamp(),
                strlen(json),
                duration
            );
        } // if
    } // if

    return result;
} // SendJsonOnWebSocket

// The WebSocket client (JavaScript) is sending data back to the ESP
void ProcessWebSocketClientMessage(const char* payload, uint32_t id)
{
    String clientMessage(payload);

    if (clientMessage == "") return;

    if (clientMessage.startsWith("in_menu:"))
    {
        // The WebSocket client is browsing through a menu
        inMenu = clientMessage.endsWith(":YES");
    }
    else if (clientMessage.startsWith("satnav_disclaimer_accepted:"))
    {
        // The WebSocket client has just accepted the sat nav disclaimer screen
        satnavDisclaimerAccepted = clientMessage.endsWith(":YES");
    }
    else if (clientMessage.startsWith("satnav_service_list_size:"))
    {
        satnavServiceListSize = clientMessage.substring(25).toInt();
    }
    else if (clientMessage.startsWith("ir_button_faster_repeat:"))
    {
        // The WebSocket client is indicating the repeat rate of the IR remote controller buttons

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
        // The WebSocket client is showing a popup

        // We need to know when a popup is showing, e.g. to know when to ignore a "MOD" button press from the
        // IR remote control.
        if (clientMessage.endsWith(":NO")) NoPopup();
        else NotificationPopupShowing(millis(), clientMessage.substring(18).toInt());
    }
    else if (clientMessage.startsWith("mfd_language:"))
    {
        // The WebSocket client passes the current language

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
        // The WebSocket client passes the current distance unit (km/h or mph)

        String value = clientMessage.substring(18);

        uint8_t prevMfdDistanceUnit = mfdDistanceUnit;
        mfdDistanceUnit =
            value == "set_units_mph" ? MFD_DISTANCE_UNIT_IMPERIAL :
            MFD_DISTANCE_UNIT_METRIC;

        if (mfdDistanceUnit != prevMfdDistanceUnit) ResetPacketPrevData();
    }
    else if (clientMessage.startsWith("mfd_temperature_unit:"))
    {
        // The WebSocket client passes the current temperature unit (Celsius or Fahrenheit)

        String value = clientMessage.substring(21);

        uint8_t prevMfdTemperatureUnit = mfdTemperatureUnit;
        mfdTemperatureUnit =
            value == "set_units_deg_fahrenheit" ? MFD_TEMPERATURE_UNIT_FAHRENHEIT :
            MFD_TEMPERATURE_UNIT_CELSIUS;

        if (mfdTemperatureUnit != prevMfdTemperatureUnit) ResetPacketPrevData();
    }
    else if (clientMessage.startsWith("mfd_time_unit:"))
    {
        // The WebSocket client passes the current time unit (12 or 24 hour)

        String value = clientMessage.substring(14);

        mfdTimeUnit =
            value == "set_units_12h" ? MFD_TIME_UNIT_12H :
            MFD_TIME_UNIT_24H;
    }

  #ifdef PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

    else if (clientMessage.startsWith("time_offset:"))
    {
        // The WebSocket client passes the current time offset from UTC

        String value = clientMessage.substring(12);
        int offsetMinutes = value.toInt();
        SetTimeZoneOffset(offsetMinutes);

        Serial.printf_P(
            PSTR("==> Time zone received from WebSocket client %lu: UTC %+2d:%02d\n"),
            id, offsetMinutes / MINS_PER_HOUR, abs(offsetMinutes % MINS_PER_HOUR)
        );
    }
    else if (clientMessage.startsWith("date_time:"))
    {
        // The WebSocket client passes the current UTC time

        String value = clientMessage.substring(10);
        uint64_t epoch_msec = atoll(value.c_str());

        uint32_t epoch = epoch_msec / 1000ULL;
        uint32_t msec = epoch_msec % 1000ULL;

        // Note: no idea how long this message has been in transport, so the time set here may be
        // off a few (hundred) milliseconds...
        if (SetTime(epoch, msec))
        {
            Serial.printf_P(
                PSTR("==> Current date-time received from WebSocket client %lu: %04d-%02d-%02d %02d:%02d:%02d.%03" PRIu32 " UTC\n"),
                id, year(epoch), month(epoch), day(epoch), hour(epoch), minute(epoch), second(epoch), msec
            );
        } // if
    } // if
  #else
    (void)id;
  #endif  // PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

} // ProcessWebSocketClientMessage

uint32_t webSocketIdJustConnected = 0;

void WebSocketEvent(
    AsyncWebSocket*,
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

            if (id == websocketId_1)
            {
                websocketId_1 = websocketId_2;
                websocketId_2 = WEBSOCKET_INVALID_ID;
            }
            else if (id == websocketId_2)
            {
                websocketId_2 = WEBSOCKET_INVALID_ID;
            } // if

            if (id == webSocketIdJustConnected)
            {
                CleanupQueuedJsons(webSocketIdJustConnected);
                webSocketIdJustConnected = 0;
            } // if

          #ifdef DEBUG_WEBSOCKET
            Serial.printf_P(PSTR("%s[webSocket] id_1=%lu, id_2=%lu\n"), TimeStamp(), websocketId_1, websocketId_2);
          #endif // DEBUG_WEBSOCKET
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
            client->client()->setAckTimeout(10000);
            //client->client()->setRxTimeout(120);

            lastWebSocketCommunication[clientIp] = millis();

            // A completely new value for id?
            if (id != websocketId_1 && id != websocketId_2)
            {
                nWebSocketConnections++;

                webSocketIdJustConnected = id;
                Serial.printf_P(PSTR(" --> will start serving %lu\n"), id);

                // Use as much as possible an empty slot
                if (websocketId_1 == WEBSOCKET_INVALID_ID || ! webSocket.hasClient(websocketId_1))
                {
                    websocketId_1 = id;
                }
                else if (websocketId_2 == WEBSOCKET_INVALID_ID || ! webSocket.hasClient(websocketId_2))
                {
                    websocketId_2 = id;
                }
                else
                {
                    // No empty slot: try to re-use the slot which served the same client (IP address)
                    IPAddress clientIp_1 = webSocket.client(websocketId_1)->remoteIP();
                    if (clientIp == clientIp_1) websocketId_1 = id; else websocketId_2 = id;
                } // if
            }
            else
            {
                Serial.printf_P(PSTR(" --> already serving %lu\n"), id);
            } // if

          #ifdef DEBUG_WEBSOCKET
            Serial.printf_P(PSTR("%s[webSocket] id_1=%lu, id_2=%lu\n"), TimeStamp(), websocketId_1, websocketId_2);
            Serial.printf_P(PSTR("%s[webSocket %" PRIu32 "] Free RAM: %" PRIu32 "\n"),
                TimeStamp(),
                id,
                system_get_free_heap_size());
          #endif // DEBUG_WEBSOCKET

            // Send ESP system data to client
            // Don't call 'SendJsonOnWebSocket' here, causes out-of-memory or stack overflow crash. Instead:
            QueueJson(EspSystemDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

            // Send equipment status data, e.g. presence of sat nav and other peripherals
            // Don't call 'SendJsonOnWebSocket' here, causes out-of-memory or stack overflow crash. Instead:
            QueueJson(EquipmentStatusDataToJson(jsonBuffer, JSON_BUFFER_SIZE));

            // Send any JSON data that was stored for later sending
            // Don't call here, causes out-of-memory or stack overflow crash
            //SendQueuedJson(websocketId_1);

            // Trigger re-sending of otherwise unchanged data
            ResetPacketPrevData();
        }
        break;

        case WS_EVT_DATA:
        {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info != nullptr && info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
            {
                // Seems safe: 'AsyncWebSocket.cpp' says (around line 697):
                //   restore byte as _handleEvent may have added a null terminator i.e., data[len] = 0;
                data[len] = '\0';

                IPAddress clientIp = client->remoteIP();
                lastWebSocketCommunication[clientIp] = millis();

                // A completely new value for id?
                if (id != websocketId_1 && id != websocketId_2)
                {
                    Serial.printf_P(
                        PSTR("%s[webSocket %lu] received text: '%s' --> switching to %lu\n"),
                        TimeStamp(), id, data, id
                    );

                    // Use as much as possible an empty slot
                    if (websocketId_1 == WEBSOCKET_INVALID_ID || ! webSocket.hasClient(websocketId_1))
                    {
                        websocketId_1 = id;
                    }
                    else if (websocketId_2 == WEBSOCKET_INVALID_ID || ! webSocket.hasClient(websocketId_2))
                    {
                        websocketId_2 = id;
                    }
                    else
                    {
                        // No empty slot: try to re-use the slot which served the same client (IP address)
                        IPAddress clientIp_1 = webSocket.client(websocketId_1)->remoteIP();
                        if (clientIp == clientIp_1) websocketId_1 = id; else websocketId_2 = id;
                    } // if

                  #ifdef DEBUG_WEBSOCKET
                    Serial.printf_P(PSTR("%s[webSocket] id_1=%lu, id_2=%lu\n"), TimeStamp(), websocketId_1, websocketId_2);
                  #endif // DEBUG_WEBSOCKET
                }
                else
                {
                  #if DEBUG_WEBSOCKET >= 2
                    Serial.printf_P(PSTR("%s[webSocket %lu] received text: '%s'\n"), TimeStamp(), id, data);
                  #endif // DEBUG_WEBSOCKET >= 2
                } // if

                ProcessWebSocketClientMessage((char*)data, id);  // Process the message
            } // if
        }
        break;

        case WS_EVT_ERROR:
        {
            uint16_t reasonCode = *(uint16_t*)arg;
            Serial.printf_P(PSTR("%s[webSocket %lu]: error %d occurred: '%s'\n"), TimeStamp(), id, reasonCode, data);
        }
        break;

        default:
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
            "\"packet_number\": \"%" PRIu32 " bytes\""
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
    static unsigned long lastPoke = 0;
    if (millis() - lastPoke >= 1000UL)  // Arithmetic has safe roll-over
    {
        lastPoke = millis();

        // Because sometimes, ESPAsyncTCP forgets about us...
      #if defined (ASYNCWEBSERVER_FORK_ESP32Async)
        for(auto& c: webSocket.getClients())
        {
            if (c.status() == WS_CONNECTED && ! c._messageQueue.empty()) c._runQueue();
        } // for
      #else
        for(const auto& c: webSocket.getClients())
        {
            if (c->status() == WS_CONNECTED && c->_messageQueue.isEmpty()) c->_runQueue();
        } // for
      #endif
    } // if

    // Somehow, webSocket.cleanupClients() sometimes causes out of memory condition
    if (system_get_free_heap_size() > 2 * 1024) webSocket.cleanupClients();

    // New WebSocket just connected?
    if (webSocketIdJustConnected != 0)
    {
        SendQueuedJson(webSocketIdJustConnected);
        CleanupQueuedJsons(webSocketIdJustConnected);
        webSocketIdJustConnected = 0;
    } // if

  #ifdef WIFI_STRESS_TEST
    static uint32_t packetNo = 0;

    if (IsIdConnected(websocketId_1)

        // Only send test frames if there is nothing else in the queue
      #if defined (ASYNCWEBSERVER_FORK_ESP32Async)
        && webSocket.client(websocketId_1)->_messageQueue.empty()
      #else
        && webSocket.client(websocketId_1)->_messageQueue.isEmpty()
      #endif
       )
    {
        bool result = SendJsonOnWebSocket(WebSocketPacketLossTestDataToJson(packetNo, jsonBuffer), false, true);
        if (result) packetNo++;
    } // if
  #endif // WIFI_STRESS_TEST

    if (millis() - lastSendQueued >= 200UL)  // Arithmetic has safe roll-over
    {
        SendQueuedJson(websocketId_1);
        SendQueuedJson(websocketId_2);
    } // if

    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup >= 5000UL)  // Arithmetic has safe roll-over
    {
        lastCleanup = millis();

        CleanupQueuedJsons();

      #ifdef DEBUG_WEBSOCKET
        Serial.printf_P(
            PSTR("%s[webSocket] %zu client%s currently connected, queued_jsons=%d, id_1=%lu, id_2=%lu, ram=%" PRIu32 "\n"),
            TimeStamp(), webSocket.count(),
            webSocket.count() == 1 ? PSTR(" is") : PSTR("s are"),
            countQueuedJsons(), websocketId_1, websocketId_2,
            system_get_free_heap_size()
        );
      #endif // DEBUG_WEBSOCKET
    } // if
} // LoopWebSocket
