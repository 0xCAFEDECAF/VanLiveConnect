Patched versions of files from https://github.com/dvarrel/ESPAsyncWebSrv

Patched from the latest (current) version 1.2.7, https://github.com/dvarrel/ESPAsyncWebSrv/releases/tag/1.2.7 .

The patch includes the following fixes:

- AsyncWebSocket.h:
  * Declare methods queueIsEmpty() and areAllQueuesEmpty()

```diff
diff ~/Arduino/libraries/ESPAsyncWebSrv/src/AsyncWebSocket.h ./src/AsyncWebSocket.h
207a208
>     bool queueIsEmpty();
261a263,265
>
>   #define ASYNCWEBSOCKET_IMPLEMENTS_ALL_QUEUES_EMPTY_METHOD
>     bool areAllQueuesEmpty();
```

- AsyncWebSocket.cpp:
  * Define methods queueIsEmpty() and areAllQueuesEmpty()
  * Fix compiler issue

```diff
diff ~/Arduino/libraries/ESPAsyncWebSrv/src/AsyncWebSocket.cpp ./src/AsyncWebSocket.cpp
542a543,547
> bool AsyncWebSocketClient::queueIsEmpty(){
>   if((_messageQueue.length() == 0) && (_status == WS_CONNECTED) ) return true;
>   return false;
> }
>
832c837
<         return IPAddress(0U);
---
>         return IPAddress(static_cast<uint32_t>(0U));
888a894,900
>   }
>   return true;
> }
>
> bool AsyncWebSocket::areAllQueuesEmpty(){
>   for(const auto& c: _clients){
>     if(! c->queueIsEmpty()) return false;
```
