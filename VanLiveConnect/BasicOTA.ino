#include <ArduinoOTA.h>

// Wifi
const char* GetHostname();

// Defined in Sleep.ino
extern unsigned long lastActivityAt;

void SetupOta()
{
    Serial.print(F("Enabling over-the-air (OTA) update ..."));

    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]; overrule here
    ArduinoOTA.setHostname(GetHostname());

    // No authentication by default
    // ArduinoOTA.setPassword((const char *)"123");

    ArduinoOTA.onStart([]()
    {
        Serial.print(F("Start\n"));
        lastActivityAt = millis();
    });
    ArduinoOTA.onEnd([]()
    {
      #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LED_ON);
      #endif

        Serial.print(F("\nEnd\n"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        int progressPerc = (progress / (total / 100));
      #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, progressPerc % 3 == 0 ? LOW : HIGH);  // Flash the LED
      #endif

        Serial.printf_P(PSTR("Progress: %u%%\r"), progressPerc);

        lastActivityAt = millis();
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
      #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LED_OFF);
      #endif

        Serial.printf_P(PSTR("Error[%u]: "), error);
        if (error == OTA_AUTH_ERROR) Serial.print(F("Auth Failed\n"));
        else if (error == OTA_BEGIN_ERROR) Serial.print(F("Begin Failed\n"));
        else if (error == OTA_CONNECT_ERROR) Serial.print(F("Connect Failed\n"));
        else if (error == OTA_RECEIVE_ERROR) Serial.print(F("Receive Failed\n"));
        else if (error == OTA_END_ERROR) Serial.print(F("End Failed\n"));
    });
    ArduinoOTA.begin();
    Serial.print(F(" OK\n"));
} // SetupOta

void LoopOta()
{
    ArduinoOTA.handle();
} // loopOta
