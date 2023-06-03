Patched versions of files from https://github.com/Links2004/arduinoWebSockets

Patched from version 2.3.6, https://github.com/Links2004/arduinoWebSockets/releases/tag/2.3.6 .

The patches include the following fixes:

- WebSockets.h:
  * Set `#define WEBSOCKETS_TCP_TIMEOUT` to `1000`
  * Set `#define WEBSOCKETS_NETWORK_TYPE` to `NETWORK_ESP8266_ASYNC`

- WebSocketsServer.cpp:
  * Disable TCP Nagle algorithm by calling `client->tcp->setNoDelay(true);` also for
    `WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC`
  * Fix spurious crash when TCP connection is lost
