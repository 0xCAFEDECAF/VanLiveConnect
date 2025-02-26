Patched versions of files from https://github.com/dvarrel/ESPAsyncWebSrv

Patched from the latest (current) version 1.2.9, https://github.com/dvarrel/ESPAsyncWebSrv/releases/tag/1.2.9 .

The patch includes the following fixes:

- AsyncWebSocket.h:
  * Change `#define WS_MAX_QUEUED_MESSAGES` value to `4` for better memory efficiency

```diff
diff ~/Arduino/libraries/ESPAsyncWebSrv/src/AsyncWebSocket.h ./src/AsyncWebSocket.h
34c34
< #define WS_MAX_QUEUED_MESSAGES 8
---
> #define WS_MAX_QUEUED_MESSAGES 4
```
