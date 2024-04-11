Patched versions of files from https://github.com/Chris--A/PrintEx

Patched from the latest (current) version 1.2.0, https://github.com/Chris--A/PrintEx/releases/tag/1.2.0 .

The patch includes the following fix:

- TypeTraits.h:
  * Fix a compilation error that occurs when using release 3.0.0 or higher of the ESP8266 Arduino core
    (https://github.com/esp8266/Arduino).

```diff
diff ~/Arduino/libraries/PrintEx/src/lib/TypeTraits.h ./src/lib/TypeTraits.h
74c74
<         select template.
---
>         select_P template.
78,79c78,79
<     template< bool V, typename T, typename U > struct select { typedef U type; };
<     template<typename T, typename U > struct select<true,T,U>{ typedef T type; };
---
>     template< bool V, typename T, typename U > struct select_P { typedef U type; };
>     template<typename T, typename U > struct select_P<true,T,U>{ typedef T type; };
84c84
<             Similar to select, however if V is false, then type is undefined.
---
>             Similar to select_P, however if V is false, then type is undefined.
```
