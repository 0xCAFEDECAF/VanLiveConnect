
// Functions for sleep (power saving) mode
//
// Inspired by:
// https://www.mischianti.org/2019/11/21/wemos-d1-mini-esp8266-the-three-type-of-sleep-mode-to-manage-energy-savings-part-4/

// Required for LIGHT_SLEEP_T delay mode
extern "C"
{
    #include "user_interface.h"
}

// Defined in Eeprom.ino
void CommitEeprom();

// Defined in IRrecv.ino
void IrDisable();

void SetupSleep()
{
    gpio_pin_wakeup_disable();
    pinMode(LIGHT_SLEEP_WAKE_PIN, INPUT_PULLUP);
} // SetupSleep

void WakeupCallback()
{
    gpio_pin_wakeup_disable();
    pinMode(LIGHT_SLEEP_WAKE_PIN, INPUT_PULLUP);

    wifi_set_sleep_type(NONE_SLEEP_T);
} // WakeupCallback

void GoToSleep()
{
    Serial.printf_P
    (
        PSTR("=====> Entering light sleep mode; will wake up when detecting VAN bus activity on pin %d\n"),
        LIGHT_SLEEP_WAKE_PIN
    );

    IrDisable();
    VanBusRx.Disable();

    WiFi.softAPdisconnect(true);

    wdt_reset();
    ESP.wdtFeed();

    CommitEeprom();

    // Wake up by pulling pin low (GND)
    gpio_pin_wakeup_enable(GPIO_ID_PIN(LIGHT_SLEEP_WAKE_PIN), GPIO_PIN_INTR_LOLEVEL);

    delay(500);

    wifi_set_opmode(NULL_MODE);
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
    wifi_fpm_open();
    wifi_fpm_set_wakeup_cb(WakeupCallback);

  #define FPM_SLEEP_MAX_TIME (0xFFFFFFF)
    wifi_fpm_do_sleep(FPM_SLEEP_MAX_TIME);

    // Execution halts here until LIGHT_SLEEP_WAKE_PIN (D1) is pulled low. Connect this pin to CANL, which is pulled
    // low (~ 0 Volt) when dominant, i.e. when VAN bus activity occurs.

    // Execution resumes here after wakeup

    delay(1000);

    // Let's go for a fresh 'n fruity start
    ESP.restart();
} // GoToSleep
