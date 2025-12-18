
// ESP system data

#ifdef ARDUINO_ARCH_ESP32
  #include "esp_system.h"
  #include "lwip/init.h"
#endif // ARDUINO_ARCH_ESP32

#ifdef PRINT_JSON_BUFFERS_ON_SERIAL
// Defined in PacketToJson.ino
void PrintJsonText(const char* jsonBuffer);
#endif // PRINT_JSON_BUFFERS_ON_SERIAL

const char PROGMEM qioStr[] = "QIO";
const char PROGMEM qoutStr[] = "QOUT";
const char PROGMEM dioStr[] = "DIO";
const char PROGMEM doutStr[] = "DOUT";
const char PROGMEM unknownStr[] = "UNKNOWN";

const String md5Checksum = ESP.getSketchMD5();

#ifndef ARDUINO_ARCH_ESP32
const uint32_t flashChipId = ESP.getFlashChipId();
const uint32_t flashSizeReal = ESP.getFlashChipRealSize();
#endif // ARDUINO_ARCH_ESP32
const uint32_t flashSizeIde = ESP.getFlashChipSize();
const FlashMode_t flashModeIde = ESP.getFlashChipMode();
const uint32_t flashChipSpeed = ESP.getFlashChipSpeed();

const char PROGMEM compileDate[] = __DATE__ ", " __TIME__;

#ifdef ARDUINO_ARCH_ESP32

 // Old Arduino ESP32 board packages do not have all the VERSION macros

 #ifndef ESP_ARDUINO_VERSION_STR
  // Current ARDUINO version, as string
  #define df2xstr(s)              #s
  #define df2str(s)               df2xstr(s)
  #define ESP_ARDUINO_VERSION_STR \
    df2str(ESP_ARDUINO_VERSION_MAJOR) "." df2str(ESP_ARDUINO_VERSION_MINOR) "." df2str(ESP_ARDUINO_VERSION_PATCH)
 #endif

const char* ResetReasonStr(esp_reset_reason_t r)
{
  switch (r)
  {
    case ESP_RST_UNKNOWN:   return "Unknown";
    case ESP_RST_POWERON:   return "PowerOn";    //Power on or RST pin toggled
    case ESP_RST_EXT:       return "ExtPin";     //External pin - not applicable for ESP32
    case ESP_RST_SW:        return "Reboot";     //esp_restart()
    case ESP_RST_PANIC:     return "Crash";      //Exception/panic
    case ESP_RST_INT_WDT:   return "WDT_Int";    //Interrupt watchdog (software or hardware)
    case ESP_RST_TASK_WDT:  return "WDT_Task";   //Task watchdog
    case ESP_RST_WDT:       return "WDT_Other";  //Other watchdog
    case ESP_RST_DEEPSLEEP: return "Sleep";      //Reset after exiting deep sleep mode
    case ESP_RST_BROWNOUT:  return "BrownOut";   //Brownout reset (software or hardware)
    case ESP_RST_SDIO:      return "SDIO";       //Reset over SDIO
    default:                return "Unknown";
  } // switch
} // ResetReasonStr

#else
String EspGetResetInfo()
{
    struct rst_info* espResetInfo = ESP.getResetInfoPtr();

    if (espResetInfo == nullptr) return String();

    static const int bufSize = 200;
    char buf[bufSize];

    // https://www.espressif.com/sites/default/files/documentation/esp8266_reset_causes_and_common_fatal_exception_causes_en.pdf#page=5
    int at = snprintf_P(buf, bufSize, PSTR("%x (%s)"),

        espResetInfo->reason,

        espResetInfo->reason == REASON_DEFAULT_RST ? PSTR("DEFAULT") :
        espResetInfo->reason == REASON_WDT_RST ? PSTR("WDT") :
        espResetInfo->reason == REASON_EXCEPTION_RST ? PSTR("EXCEPTION") :
        espResetInfo->reason == REASON_SOFT_WDT_RST ? PSTR("SOFT_WDT") :
        espResetInfo->reason == REASON_SOFT_RESTART ? PSTR("SOFT_RESTART") :
        espResetInfo->reason == REASON_DEEP_SLEEP_AWAKE ? PSTR("DEEP_SLEEP_AWAKE") :
        espResetInfo->reason == REASON_EXT_SYS_RST ? PSTR("EXT_SYS_RST") :
        PSTR("???")
    );

    if (espResetInfo->reason == REASON_WDT_RST ||
        espResetInfo->reason == REASON_EXCEPTION_RST ||
        espResetInfo->reason == REASON_SOFT_WDT_RST)
    {
        if (espResetInfo->reason == REASON_EXCEPTION_RST)
        {
            // https://links2004.github.io/Arduino/dc/deb/md_esp8266_doc_exception_causes.html
            // https://www.espressif.com/sites/default/files/documentation/esp8266_reset_causes_and_common_fatal_exception_causes_en.pdf#page=6
            at += at >= bufSize ? 0 :
                snprintf_P(buf + at, bufSize - at, PSTR(" Fatal exception (%d):"), espResetInfo->exccause);
        } // if

        at += at >= bufSize ? 0 :
            snprintf_P(buf + at, bufSize - at,
                PSTR(" epc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x"),
                espResetInfo->epc1,
                espResetInfo->epc2,
                espResetInfo->epc3,
                espResetInfo->excvaddr,
                espResetInfo->depc
            );
    } // if

    return String(buf);
} // EspGetResetInfo
#endif // ARDUINO_ARCH_ESP32

void PrintSystemSpecs()
{
  #ifdef ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("Reset reason: %s\n"), ResetReasonStr(esp_reset_reason()));
    Serial.printf_P(PSTR("CPU Speed: %" PRIu32 " MHz (CPU_F_FACTOR = %ld)\n"), ESP.getCpuFreqMHz(), CPU_F_FACTOR);
    Serial.printf_P(PSTR("Arduino ESP32 chip model: %s\n"), CONFIG_IDF_TARGET);
    Serial.printf_P(PSTR("Arduino ESP32 board package version: %s\n"), ESP_ARDUINO_VERSION_STR);

  #else // ! ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("Reset reason: %s\n"), EspGetResetInfo().c_str());
    Serial.printf_P(PSTR("CPU Speed: %" PRIu32 " MHz (CPU_F_FACTOR = %ld)\n"), ESP.getCpuFreqMHz(), CPU_F_FACTOR);

   #if defined ARDUINO_ESP8266_RELEASE
    Serial.printf_P(PSTR("Arduino ESP8266 board package version: %s\n"), ARDUINO_ESP8266_RELEASE);
   #elif defined ARDUINO_ESP8266_DEV
    Serial.printf_P(PSTR("Arduino ESP8266 board package version: DEV\n"));
   #else
    Serial.printf_P(PSTR("Arduino ESP8266 board package version: UNKNOWN\n"));
   #endif

  #endif // ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("\"NONOS\" SDK version: %s\n"), ESP.getSdkVersion());
    Serial.printf_P(PSTR("lwIP (lightweight IP) version: %s\n"), LWIP_VERSION_STRING);

    char floatBuf[MAX_FLOAT_SIZE];
  #ifndef ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("Flash real size: %s MBytes\n"), FloatToStr(floatBuf, flashSizeReal/1024.0/1024.0, 2));
  #endif // ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("Flash ide size: %s MBytes\n"), FloatToStr(floatBuf, flashSizeIde/1024.0/1024.0, 2));
    Serial.printf_P(PSTR("Flash ide speed: %s MHz\n"), FloatToStr(floatBuf, ESP.getFlashChipSpeed()/1000000.0, 2));
    FlashMode_t ideMode = ESP.getFlashChipMode();
    Serial.printf_P(PSTR("Flash ide mode: %s\n"),
        ideMode == FM_QIO ? qioStr :
        ideMode == FM_QOUT ? qoutStr :
        ideMode == FM_DIO ? dioStr :
        ideMode == FM_DOUT ? doutStr :
        unknownStr);
  #ifndef ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("Flash chip configuration %s\n"), flashSizeIde != flashSizeReal ? PSTR("wrong!") : PSTR("ok."));
  #endif // ARDUINO_ARCH_ESP32

    Serial.print(F("Software image MD5 checksum: "));
    Serial.print(md5Checksum);
    Serial.printf_P(PSTR(" (%s)\n"), compileDate);

    Serial.print(F("Wi-Fi MAC address: "));
    Serial.print(Network.macAddress());
  #ifdef ON_DESK_MFD_ESP_MAC
    if (Network.macAddress() == ON_DESK_MFD_ESP_MAC)
    {
        Serial.print(F(" == ON_DESK_MFD_ESP_MAC, i.e., this is the on-desk test setup, so:\n"));
        Serial.print(F("==> Will print detailed debug info when a VAN bus packet with CRC error is received.\n"));
        Serial.print(F("==> Will go to sleep quicker if no bus activity is detected."));
    }
  #endif // ON_DESK_MFD_ESP_MAC
    Serial.print("\n");
} // PrintSystemSpecs

const char* EspSystemDataToJson(char* buf, const int n)
{
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"esp_last_reset_reason\": \"%s\",\n"
          #ifndef ARDUINO_ARCH_ESP32
            "\"esp_last_reset_info\": \"%s\",\n"
            "\"esp_boot_version\": \"%u\",\n"
          #endif // ARDUINO_ARCH_ESP32
            "\"esp_cpu_speed\": \"%" PRIu32 " MHz\",\n"
            "\"esp_sdk_version\": \"%s\",\n"
          #ifdef ARDUINO_ARCH_ESP32
            "\"esp_chip_id\": \"0x%016" PRIX64 "\",\n"
          #else
            "\"esp_chip_id\": \"0x%08X\",\n"
            "\"esp_flash_id\": \"0x%08X\",\n"
            "\"esp_flash_size_real\": \"%s MBytes\",\n"
          #endif // ARDUINO_ARCH_ESP32
            "\"esp_flash_size_ide\": \"%s MBytes\",\n"
            "\"esp_flash_speed_ide\": \"%s MHz\",\n"

            "\"esp_flash_mode_ide\": \"%s\",\n"

            "\"esp_mac_address\": \"%s\",\n"
            "\"esp_ip_address\": \"%s\",\n"
            "\"esp_wifi_rssi\": \"%d dB\",\n"

            "\"esp_free_ram\": \"%" PRIu32 " bytes\",\n"

            "\"img_md5_checksum\": \"%s\",\n"
            "\"img_compile_date\": \"%s\"\n"
        "}\n"
    "}\n";

    char floatBuf[3][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

      #ifdef ARDUINO_ARCH_ESP32
        ResetReasonStr(esp_reset_reason()),
      #else
        ESP.getResetReason().c_str(),
      #endif // ARDUINO_ARCH_ESP32

      #ifndef ARDUINO_ARCH_ESP32
        EspGetResetInfo().c_str(),
        ESP.getBootVersion(),
      #endif // ARDUINO_ARCH_ESP32

        ESP.getCpuFreqMHz(), // system_get_cpu_freq(),
        ESP.getSdkVersion(),
      #ifdef ARDUINO_ARCH_ESP32
        ESP.getEfuseMac(),
      #else
        ESP.getChipId(),
        flashChipId,
        FloatToStr(floatBuf[0], flashSizeReal/1024.0/1024.0, 2),
      #endif // ARDUINO_ARCH_ESP32
        FloatToStr(floatBuf[1], flashSizeIde/1024.0/1024.0, 2),
        FloatToStr(floatBuf[2], flashChipSpeed/1000000.0, 2),

        flashModeIde == FM_QIO ? qioStr :
        flashModeIde == FM_QOUT ? qoutStr :
        flashModeIde == FM_DIO ? dioStr :
        flashModeIde == FM_DOUT ? doutStr :
        unknownStr,

        Network.macAddress().c_str(),
      #ifdef WIFI_AP_MODE // Wi-Fi access point mode
        IP_ADDR,
      #else
        WiFi.localIP().toString().c_str(),
      #endif // ifdef WIFI_AP_MODE
        WiFi.RSSI(),

        system_get_free_heap_size(),

        md5Checksum.c_str(),
        compileDate
    );

    // JSON buffer overflow?
    if (at >= n) return "";

  #ifdef PRINT_JSON_BUFFERS_ON_SERIAL
    Serial.printf_P(PSTR("%sESP system data as JSON object:\n"), TimeStamp());
    PrintJsonText(buf);
  #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return buf;
} // EspSystemDataToJson

const char* EspRuntimeDataToJson(char* buf, const int n)
{
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"uptime_seconds\": \"%lu\",\n"
            "\"esp_wifi_rssi\": \"%d dB\",\n"
            "\"esp_free_ram\": \"%" PRIu32 " bytes\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter, millis() / 1000, WiFi.RSSI(), system_get_free_heap_size());

    // JSON buffer overflow?
    if (at >= n) return "";

  #ifdef PRINT_JSON_BUFFERS_ON_SERIAL
    Serial.printf_P(PSTR("%sESP runtime data as JSON object:\n"), TimeStamp());
    PrintJsonText(buf);
  #endif  // PRINT_JSON_BUFFERS_ON_SERIAL

    return buf;
} // EspRuntimeDataToJson
