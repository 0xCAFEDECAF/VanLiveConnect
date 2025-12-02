
// Functions for sleep (power saving) mode
//
// Inspired by:
// https://www.mischianti.org/2019/11/21/wemos-d1-mini-esp8266-the-three-type-of-sleep-mode-to-manage-energy-savings-part-4/

#ifdef ARDUINO_ARCH_ESP32
  #include "driver/rtc_io.h"
  #include <esp_task_wdt.h>
#else
  // Required for LIGHT_SLEEP_T delay mode
  extern "C"
  {
    #include "user_interface.h"
  }
#endif // ARDUINO_ARCH_ESP32

// Defined in Eeprom.ino
void CommitEeprom();

// Defined in IRrecv.ino
void IrDisable();

unsigned long lastActivityAt = 0;

void SetupSleep()
{
  #ifdef ARDUINO_ARCH_ESP32
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // Configure pullup/downs via RTCIO to tie wakeup pins to inactive level during deepsleep.
    // EXT0 resides in the same power domain (RTC_PERIPH) as the RTC IO pullup/downs.
    // No need to keep that power domain explicitly, unlike EXT1.
    rtc_gpio_pullup_en(LIGHT_SLEEP_WAKE_PIN);
    rtc_gpio_pulldown_dis(LIGHT_SLEEP_WAKE_PIN);

  #else
    gpio_pin_wakeup_disable();
    pinMode(LIGHT_SLEEP_WAKE_PIN, INPUT_PULLUP);

  #endif // ARDUINO_ARCH_ESP32
} // SetupSleep

#ifndef ARDUINO_ARCH_ESP32
void WakeupCallback()
{
    gpio_pin_wakeup_disable();
    pinMode(LIGHT_SLEEP_WAKE_PIN, INPUT_PULLUP);

    // Seems to help in decreasing the jitter on the VAN bus bit timings. But also seems to
    // deteriorate Wi-Fi connectivity.
    //wifi_set_sleep_type(NONE_SLEEP_T);
} // WakeupCallback
#endif // ARDUINO_ARCH_ESP32

void GoToSleep()
{
    Serial.printf_P
    (
        PSTR("%s===> Entering light sleep mode; will wake up when detecting VAN bus activity on pin %s (GPIO%u)\n"),
        TimeStamp(),
        XSTR(LIGHT_SLEEP_WAKE_PIN),
        LIGHT_SLEEP_WAKE_PIN
    );
    Serial.flush();

    IrDisable();
    VanBusRx.Disable();

  #ifndef ARDUINO_ARCH_ESP32
    ESP.wdtFeed();
  #endif // ARDUINO_ARCH_ESP32

    CommitEeprom();

    delay(1000);

    // TODO - need this?
    //pinMode(LIGHT_SLEEP_WAKE_PIN, WAKEUP_PULLUP);

    // Wake up by pulling pin low (GND)
  #ifdef ARDUINO_ARCH_ESP32
    esp_sleep_enable_ext0_wakeup(LIGHT_SLEEP_WAKE_PIN, 0);  //1 = High, 0 = Low
  #else
    gpio_pin_wakeup_enable(GPIO_ID_PIN(LIGHT_SLEEP_WAKE_PIN), GPIO_PIN_INTR_LOLEVEL);
  #endif // ARDUINO_ARCH_ESP32

    delay(1000);

  #ifdef ARDUINO_ARCH_ESP32
    esp_deep_sleep_start();
  #else
    wifi_set_opmode(NULL_MODE);
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
    wifi_fpm_open();
    wifi_fpm_set_wakeup_cb(WakeupCallback);

  #define FPM_SLEEP_MAX_TIME (0xFFFFFFF)
    wifi_fpm_do_sleep(FPM_SLEEP_MAX_TIME);
  #endif // ARDUINO_ARCH_ESP32

    // Execution halts here until LIGHT_SLEEP_WAKE_PIN (see Config.h) is pulled low. Connect this pin to CANL, which
    // is pulled low (~ 0 Volt) when dominant, i.e. when VAN bus activity occurs.

    // Execution resumes here after wakeup

    delay(100);

    Serial.printf_P
    (
        PSTR("\n===> VAN bus activity on pin %s (GPIO%u) detected: waking up from light sleep mode\n"),
        XSTR(LIGHT_SLEEP_WAKE_PIN),
        LIGHT_SLEEP_WAKE_PIN
    );
    Serial.flush();

    delay(500);

    // Let's go for a fresh 'n fruity start
    ESP.restart();
} // GoToSleep
