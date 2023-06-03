Patched versions of files from https://github.com/me-no-dev/ESPAsyncTCP

Patched from the latest (current) version, https://github.com/me-no-dev/ESPAsyncTCP/tree/15476867dcbab906c0f1d47a7f63cdde223abeab .

The patches include the following fixes:

- ESPAsyncTCPbuffer.cpp:
  * Possible fix for issue causing soft watchdog timeout resets
  * Add public method AsyncTCPbuffer::setNoDelay
  * Fix issue causing missing of response, possibly fix for hanging TCP connections

- ESPAsyncTCP.cpp:
  * Possible fix for memory leak
