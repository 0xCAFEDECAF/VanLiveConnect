Patched versions of files from https://github.com/me-no-dev/ESPAsyncTCP

Patched from the latest (current) version, https://github.com/me-no-dev/ESPAsyncTCP/tree/15476867dcbab906c0f1d47a7f63cdde223abeab .

The patches include the following fixes:

- ESPAsyncTCPbuffer.cpp:
  * Possible fix for issue causing soft watchdog timeout resets
  * Fix issue causing missing of response, possibly fix for hanging TCP connections

```diff
diff ~/Arduino/libraries/ESPAsyncTCP/src/ESPAsyncTCPbuffer.cpp ./src/ESPAsyncTCPbuffer.cpp
113c113,115
<         _sendBuffer();
---
>
>         // Seems to fix soft watchdog timeout resets
>         if (_client->canSend()) _sendBuffer();
389c399,400
<     if(!_client || !_client->connected()) {
---
>     //FIX do not check connected here, otherwise miss responce
>     if(!_client /*|| !_client->connected()*/) {
450c461,462
<     if(!_client || !_client->connected() || _RXbuffer == NULL) {
---
>     //FIX do not check connected here, otherwise miss responce
>     if(!_client /*|| !_client->connected()*/ || _RXbuffer == NULL) {
```

- ESPAsyncTCP.cpp:
  * Possible fix for memory leak

```diff
diff ~/Arduino/libraries/ESPAsyncTCP/src/ESPAsyncTCP.cpp ./src/ESPAsyncTCP.cpp
267a268,270
>   if (err != ERR_OK){
>     tcp_close(pcb);
>   }
```
