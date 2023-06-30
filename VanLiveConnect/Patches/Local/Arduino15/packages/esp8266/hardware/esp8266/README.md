The files inside these sub-directories are patched versions of their originals as found
on the Internet.

Copy the patched files into their corresponding directory/ies inside:

- Windows: `%USERPROFILE%\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\`

- Linux: `$HOME/.arduino15/packages/esp8266/hardware/esp8266/`

The patches include the following fixes:

- platform.txt:
  * Set `build.sdk` to `NONOSDK22x_191122`
  * Define `PUYA_SUPPORT` to support SPI-FFS in specific hardware

```diff
diff ~/.arduino15/packages/esp8266/hardware/esp8266/2.6.3/platform.txt ./2.6.3/
45c45
< build.sdk=NONOSDK22x_190703
---
> #build.sdk=NONOSDK22x_190703
47a48
> build.sdk=NONOSDK22x_191122
67c68
< compiler.cpp.flags=-c {compiler.warning_flags} -Os -g -mlongcalls -mtext-section-literals -fno-rtti -falign-functions=4 {build.stdcpp_level} -MMD -ffunction-sections -fdata-sections {build.exception_flags} {build.sslflags}
---
> compiler.cpp.flags=-c {compiler.warning_flags} -Os -g -mlongcalls -mtext-section-literals -fno-rtti -falign-functions=4 {build.stdcpp_level} -MMD -ffunction-sections -fdata-sections {build.exception_flags} {build.sslflags} -DPUYA_SUPPORT
```

- spiffs_config.h:
  * Set `#define SPIFFS_READ_ONLY` to `1`

```diff
diff ~/.arduino15/packages/esp8266/hardware/esp8266/2.6.3/cores/esp8266/spiffs/spiffs_config.h ./2.6.3/cores/esp8266/spiffs/spiffs_config.h
279c279
< #define SPIFFS_READ_ONLY                      0
---
> #define SPIFFS_READ_ONLY                      1
```

- spiffs_api.h:
  * Fix a compilation error that occurs when defining `SPIFFS_READ_ONLY`

```diff
diff ~/.arduino15/packages/esp8266/hardware/esp8266/2.6.3/cores/esp8266/spiffs_api.h ./2.6.3/cores/esp8266/spiffs_api.h
456a457
>       #ifndef SPIFFS_READ_ONLY
461a463
>       #endif
462a465
>       #ifndef SPIFFS_READ_ONLY
463a467
>       #endif
```
