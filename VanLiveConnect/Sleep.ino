
// Functions for sleep (power saving) mode
//
// Inspired by:
// https://www.mischianti.org/2019/11/21/wemos-d1-mini-esp8266-the-three-type-of-sleep-mode-to-manage-energy-savings-part-4/

// Required for LIGHT_SLEEP_T delay mode
extern "C"
{
    #include "user_interface.h"
}

// Connect to "CANL" pin of the MCP2551 board for waking up at VAN bus activity
// Notes:
// - Call "pinMode(..., INPUT_PULLUP);" and "gpio_pin_wakeup_enable(..., GPIO_PIN_INTR_LOLEVEL);"
// - ESP8266 board seems to cope well with +5V voltage levels on input pins; see also:
//   https://ba0sh1.com/2016/08/03/is-esp8266-io-really-5v-tolerant/
#define LIGHT_WAKE_PIN D1

void SetupSleep()
{
    gpio_pin_wakeup_disable();

    // Pass 'GPIO_PIN_INTR_LOLEVEL' to 'gpio_pin_wakeup_enable'
    pinMode(LIGHT_WAKE_PIN, INPUT_PULLUP);

    VanBusRx.Enable();
} // SetupSleep

void WakeupCallback()
{
    Serial.println(F("=====> Wakeup callback"));
    Serial.flush();
} // WakeupCallback

void GoToSleep()
{
    Serial.printf_P(PSTR("=====> Entering light sleep mode; monitoring VAN bus activity on pin %d\n"), LIGHT_WAKE_PIN);

    VanBusRx.Disable();
 
    // Wake up by pulling pin low (GND)
    gpio_pin_wakeup_enable(GPIO_ID_PIN(LIGHT_WAKE_PIN), GPIO_PIN_INTR_LOLEVEL);

    wifi_set_opmode(NULL_MODE);
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
    wifi_fpm_open();
    wifi_fpm_set_wakeup_cb(WakeupCallback);

  #define FPM_SLEEP_MAX_TIME (0xFFFFFFF)
    wifi_fpm_do_sleep(FPM_SLEEP_MAX_TIME);

    SetupSleep();
    delay(1000);
    Serial.println(F("=====> VAN bus activity detected; exiting light sleep mode"));
} // GoToSleep
