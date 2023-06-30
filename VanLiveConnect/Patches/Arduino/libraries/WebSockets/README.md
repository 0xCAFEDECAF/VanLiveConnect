Patched versions of files from https://github.com/Links2004/arduinoWebSockets

Patched from version 2.3.6, https://github.com/Links2004/arduinoWebSockets/releases/tag/2.3.6 .

The patches include the following fixes:

- WebSockets.h:
  * Set `#define WEBSOCKETS_TCP_TIMEOUT` to `1000`
  * Set `#define WEBSOCKETS_NETWORK_TYPE` to `NETWORK_ESP8266_ASYNC`

```diff
diff -r ~/Arduino/libraries/WebSockets/src/WebSockets.h ./src/WebSockets.h
44a45,46
> //#define DEBUG_ESP_PORT Serial
>
96a99
> // Don't go lower than (500)
97a101
> //#define WEBSOCKETS_TCP_TIMEOUT (500)
113,114c117,118
< #define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP8266
< //#define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP8266_ASYNC
---
> //#define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP8266
> #define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP8266_ASYNC
```

- WebSocketsServer.cpp:
  * Disable TCP Nagle algorithm by calling `client->tcp->setNoDelay(true);` also for
    `WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC`
  * Fix spurious crash when TCP connection is lost

```diff
diff -r ~/Arduino/libraries/WebSockets/src/WebSocketsServer.cpp ./src/WebSocketsServer.cpp
432a433,434
>             client->tcp->setNoDelay(true);
>
435d436
<             client->tcp->setNoDelay(true);
452a454
>                 if (! client) return false;
```
