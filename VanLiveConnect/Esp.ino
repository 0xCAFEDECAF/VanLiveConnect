
// ESP system data

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

const uint32_t flashChipId = ESP.getFlashChipId();
const uint32_t flashSizeReal = ESP.getFlashChipRealSize();
const uint32_t flashSizeIde = ESP.getFlashChipSize();
const FlashMode_t flashModeIde = ESP.getFlashChipMode();
const uint32_t flashChipSpeed = ESP.getFlashChipSpeed();

const char PROGMEM compileDate[] = __DATE__ " " __TIME__;

void PrintSystemSpecs()
{
    Serial.printf_P(PSTR("CPU Speed: %u MHz (CPU_F_FACTOR = %d)\n"), system_get_cpu_freq(), CPU_F_FACTOR);
    Serial.printf_P(PSTR("SDK: %s\n"), system_get_sdk_version());

    uint32_t realSize = ESP.getFlashChipRealSize();
    uint32_t ideSize = ESP.getFlashChipSize();

    char floatBuf[MAX_FLOAT_SIZE];
    Serial.printf_P(PSTR("Flash real size: %s MBytes\n"), FloatToStr(floatBuf, realSize/1024.0/1024.0, 2));
    Serial.printf_P(PSTR("Flash ide size: %s MBytes\n"), FloatToStr(floatBuf, ideSize/1024.0/1024.0, 2));
    Serial.printf_P(PSTR("Flash ide speed: %s MHz\n"), FloatToStr(floatBuf, ESP.getFlashChipSpeed()/1000000.0, 2));
    FlashMode_t ideMode = ESP.getFlashChipMode();
    Serial.printf_P(PSTR("Flash ide mode: %S\n"),
        ideMode == FM_QIO ? qioStr :
        ideMode == FM_QOUT ? qoutStr :
        ideMode == FM_DIO ? dioStr :
        ideMode == FM_DOUT ? doutStr :
        unknownStr);
    Serial.printf_P(PSTR("Flash chip configuration %S\n"), ideSize != realSize ? PSTR("wrong!") : PSTR("ok."));

    Serial.print(F("Software image MD5 checksum: "));
    Serial.print(md5Checksum);
    Serial.printf_P(PSTR(" (%S)\n"), compileDate);

    Serial.print(F("Wi-Fi MAC address: "));
    Serial.print(WiFi.macAddress());
  #ifdef ON_DESK_MFD_ESP_MAC
    if (WiFi.macAddress() == ON_DESK_MFD_ESP_MAC)
    {
        Serial.println(F(" == ON_DESK_MFD_ESP_MAC, i.e., this is the on-desk test setup, so:"));
        Serial.print(F("==> Will print detailed debug info when a VAN bus packet with CRC error is received."));
    }
  #endif // ON_DESK_MFD_ESP_MAC
    Serial.println();
} // PrintSystemSpecs

const char* EspSystemDataToJson(char* buf, const int n)
{
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"esp_last_reset_reason\": \"%s\",\n"
            "\"esp_last_reset_info\": \"%s\",\n"
            "\"esp_boot_version\": \"%u\",\n"
            "\"esp_cpu_speed\": \"%u MHz\",\n"
            "\"esp_sdk_version\": \"%s\",\n"
            "\"esp_chip_id\": \"0x%08X\",\n"

            "\"esp_flash_id\": \"0x%08X\",\n"
            "\"esp_flash_size_real\": \"%s MBytes\",\n"
            "\"esp_flash_size_ide\": \"%s MBytes\",\n"
            "\"esp_flash_speed_ide\": \"%s MHz\",\n"

            "\"esp_flash_mode_ide\": \"%S\",\n"

            "\"esp_mac_address\": \"%s\",\n"
            "\"esp_ip_address\": \"%s\",\n"
            "\"esp_wifi_rssi\": \"%d dB\",\n"

            "\"esp_free_ram\": \"%u bytes\",\n"

            "\"img_md5_checksum\": \"%s\",\n"
            "\"img_compile_date\": \"%S\"\n"
        "}\n"
    "}\n";

    char floatBuf[3][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

        ESP.getResetReason().c_str(),
        ESP.getResetInfo().c_str(),

        ESP.getBootVersion(),
        ESP.getCpuFreqMHz(), // system_get_cpu_freq(),
        ESP.getSdkVersion(),
        ESP.getChipId(),

        flashChipId,
        FloatToStr(floatBuf[0], flashSizeReal/1024.0/1024.0, 2),
        FloatToStr(floatBuf[1], flashSizeIde/1024.0/1024.0, 2),
        FloatToStr(floatBuf[2], flashChipSpeed/1000000.0, 2),

        flashModeIde == FM_QIO ? qioStr :
        flashModeIde == FM_QOUT ? qoutStr :
        flashModeIde == FM_DIO ? dioStr :
        flashModeIde == FM_DOUT ? doutStr :
        unknownStr,

        WiFi.macAddress().c_str(),
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
    Serial.print(F("Parsed to JSON object:\n"));
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
            "\"esp_wifi_rssi\": \"%d dB\",\n"
            "\"esp_free_ram\": \"%u bytes\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,
        WiFi.RSSI(),
        system_get_free_heap_size()
    );

    // JSON buffer overflow?
    if (at >= n) return "";

  #ifdef PRINT_JSON_BUFFERS_ON_SERIAL
    Serial.print(F("Parsed to JSON object:\n"));
    PrintJsonText(buf);
  #endif  // PRINT_JSON_BUFFERS_ON_SERIAL

    return buf;
} // EspRuntimeDataToJson
