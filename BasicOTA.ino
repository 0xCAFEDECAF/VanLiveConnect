#include <ArduinoOTA.h>

// Wifi
const char* GetHostname();

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
        Serial.println(F("Start"));
    });
    ArduinoOTA.onEnd([]()
    {
        digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on
        Serial.println(F("\nEnd"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        int progressPerc = (progress / (total / 100));
        digitalWrite(LED_BUILTIN, progressPerc % 3 == 0 ? LOW : HIGH);  // Flash the LED

        Serial.printf_P(PSTR("Progress: %u%%\r"), progressPerc);
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

        char buffer[20];
        sprintf_P(buffer, PSTR("Error[%u]: "), error);
        Serial.print(buffer);
        if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
        else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
        else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
        else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
        else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
    });
    ArduinoOTA.begin();
    Serial.println(F(" OK"));
} // setupOta

void LoopOta()
{
    ArduinoOTA.handle();
} // loopOta
