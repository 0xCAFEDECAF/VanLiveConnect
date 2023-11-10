#ifndef Config_h
#define Config_h

#include <ESP8266WiFi.h>

// -----
// Compilation

// Print warning / error messages for unadvised values of compile-time #defines
#define STRICT_COMPILATION

// -----
// Wi-Fi and IP configuration

#define HOST_NAME "VanLiveConnect"

// Define to use Wi-Fi "access point" (AP) mode. Comment out to use Wi-Fi station (client) mode
#define WIFI_AP_MODE

// Define when using DHCP; comment out when using a static (fixed) IP address.
// Note: only applicable in Wi-Fi station (client) mode.
#define USE_DHCP

// Define when using a Windows Internet Connection Sharing (ICS) Wi-Fi. Comment out when using Android Wi-Fi hotspot.
// Note: only applicable when using a static (fixed) IP address, not when using DHCP.
//#define WINDOWS_ICS

#ifdef WIFI_AP_MODE // Wi-Fi access point mode
  #define WIFI_SSID "PSA display AP"
  #define WIFI_SSID_HIDDEN (0)  // Set to (1) to have a hidden SSID

  // Define to set password on access point
  //
  // Notes:
  // - Many devices will only automatically connect to a Wi-Fi access point if it has a password on it.
  // - ESP requires more memory when WIFI_PASSWORD is defined, so it may be necessary to undefine this when
  //   setting one or more debug #defines.
  //
  //#define WIFI_PASSWORD "99999999"

  // Set between (1) and (13). Note: some devices cannot connect to channels above 11. Channel 6 seems best for
  // avoiding interference with Bluetooth.
  #define WIFI_CHANNEL (8)

  #define IP_ADDR "192.168.244.1"

#else  // Wi-Fi station (client) mode
  #define WIFI_SSID "WiFiHotspot"  // Choose yours
  #define WIFI_PASSWORD "WiFiPass"  // Fill in yours

  #ifdef USE_DHCP

    // Using DHCP; ESP will register HOST_NAME via DHCP option 12.
    // Note: Neither Windows ICS nor Android Wi-Fi hotspot seem to support registering the host name on their
    // DHCP server implementation. Moreover, Windows ICS DHCP will NOT assign the previously assigned IP address to
    // the same MAC address upon new connection.

  #else // ! USE_DHCP

    // Using static (fixed) IP configuration (not DHCP); hostname will not be registered.

    #ifdef WINDOWS_ICS  // When using a Windows ICS hotspot
      #define IP_ADDR "192.168.137.2"
      #define IP_GATEWAY "192.168.137.1"
      #define IP_SUBNET "255.255.255.0"

    #else  // When using an Android hotspot
      #define IP_ADDR "192.168.43.2"
      #define IP_GATEWAY "192.168.43.1" // Dummy value, actual GW can be on any address within the subnet
      #define IP_SUBNET "255.255.255.0"

    #endif // ifdef WINDOWS_ICS

  #endif // ifdef USE_DHCP

#endif // ifdef WIFI_AP_MODE

// Define to use the mDNS responder
//#define USE_MDNS

// -----
// Sleep and wake up

// After 5 minutes (5 * 60,000 milliseconds) of VAN bus inactivity, go to sleep to save power.
// Set to (-1) to disable sleep mode altogether.
#define SLEEP_MS_AFTER_NO_VAN_BUS_ACTIVITY (5 * 60000UL)

// Connect the following pin to "VAN DATA" (in the given schematics that is "CANL") of the MCP2551 board, for waking
// up at VAN bus activity.
//
// Notes:
//
// - CANL is pulled low (~ 0 Volt) when dominant, and is typically biased between 2.0 - 3.0 Volt when recessive.
//   See e.g.:
//   * http://ww1.microchip.com/downloads/en/devicedoc/21667d.pdf
//     --> Page 9, parameter D7 ("V-CANH", "V-CANL"): Min = 2 V, Max = 3 V.
//   * https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf?ts=1592992149874
//     --> Page 7, parameter V-OL: typ 2.3 V.
//   * https://www.ti.com/lit/an/slla337/slla337.pdf?HQS=slla337-aaj&ts=1668604399231&ref_url=https%253A%252F%252Fwww.google.se%252F
//     --> Page 3, Figure 2.
//   But just in case, ESP8266 board *does* seem to cope well with +5V voltage levels on input pins; see also:
//   https://ba0sh1.com/2016/08/03/is-esp8266-io-really-5v-tolerant/
//
// - The following clamping circuit is required to decouple the ESP from transient spikes that may occur on the VAN bus:
//
//     CANL ---[R]---\
//                   |
//      GND ---|>|---+---|>|--- +5V on ESP board
//
//      GND ---|>|--- +5V
//
//   where:
//     ---[R]--- = resistor 470 Ohm
//     ---|>|--- = diode 1N4148
//
// - In the test setup on the desk, best not to power off using the main power switch; this sometimes causes
//   the ESP to become completely unresponsive (caused by ground loop??). Instead, just disconnect the +12V line
//   to simulate a power-off event.
//
#define LIGHT_SLEEP_WAKE_PIN D1

// -----
// Web server

// Define SERVE_FROM_SPIFFS to make the web server to use the SPI Flash File system (SPIFFS) to serve its web
// documents from. When SERVE_FROM_SPIFFS is not defined (commented out), the web server serves the web documents
// from program space.
//
// Notes:
//
// - To upload the web document files to the SPIFFS on the ESP8266, you will need the "Arduino ESP8266
//   filesystem uploader", as found at https://github.com/esp8266/arduino-esp8266fs-plugin/releases .
//   See also https://randomnerdtutorials.com/install-esp8266-filesystem-uploader-arduino-ide/ for installation
//   instructions.
//
// - The web documents to be uploaded to SPIFFS are already present in the expected directory 'data'.
//
// - Using SPIFFS as it is shipped by default in the esp8266/Arduino releases (https://github.com/esp8266/Arduino)
//   causes many VAN packets have CRC errors. The solution is to use SPIFFS in "read-only" mode. For that,
//   patch the following file (on Windows; in Linux the path will be slightly different but similar):
//
//     c:\Users\<user_id>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.6.3\cores\esp8266\spiffs\spiffs_config.h
//
//   Change the SPIFFS_READ_ONLY define to:
//
//     #define SPIFFS_READ_ONLY                      1
//
//   That will break compilation, which is solved by patching the file:
//
//     c:\Users\<user_id>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.6.3\cores\esp8266\spiffs_api.h
//
//   Change the 'truncate' function as follows:
//
//     bool truncate(uint32_t size) override
//     {
//       #ifndef SPIFFS_READ_ONLY
//         CHECKFD();
//         spiffs_fd *sfd;
//         if (spiffs_fd_get(_fs->getFs(), _fd, &sfd) == SPIFFS_OK) {
//             return SPIFFS_OK == spiffs_object_truncate(sfd, size, 0);
//         } else {
//       #endif
//           return false;
//       #ifndef SPIFFS_READ_ONLY
//         }
//       #endif
//    }
//
//#define SERVE_FROM_SPIFFS

// -----
// Define to disable (gray-out) the navigation menu while driving.
//
// Using the "Peugeot Planet 2000" software, the multi-functional display (MFD) parameter
// "setting the parameters of the navigation function while driving" can be set to either "Active"
// (allow for entering the navigation menu while driving) or "Inactive" (navigation menu grayed out
// while driving). Choose the following define to match the parameter as provisioned in your MFD.
//
//#define MFD_DISABLE_NAVIGATION_MENU_WHILE_DRIVING

// -----
// Test setup on desk
// If this is defined, run-time behavior will be a bit different if the ESP has this MAC address
//#define ON_DESK_MFD_ESP_MAC "48:3F:DA:01:DB:85"

// -----
// Infrared receiver

// Choose one of these #defines below (or define your own IR_RECV_PIN, IR_VCC and IR_GND)
#define IR_TSOP48XX
//#define IR_TSOP312XX

// TSOP48XX
#ifdef IR_TSOP48XX

  // IR receiver data pin
  #define IR_RECV_PIN D5

  // Using D7 as VCC and D6 as ground pin for the IR receiver. Should be possible with e.g. the
  // TSOP4838 IR receiver as it typically uses only 0.7 mA (maximum GPIO current is 12 mA;
  // see https://tttapa.github.io/ESP8266/Chap04%20-%20Microcontroller.html for ESP8266 and
  // https://esp32.com/viewtopic.php?f=2&t=2027 for ESP32).
  #define IR_VCC D7
  #define IR_GND D6

#endif // IR_TSOP48XX

// TSOP312XX
#ifdef IR_TSOP312XX

  // IR receiver data pin
  #define IR_RECV_PIN D7

  // Using D7 as VCC and D6 as ground pin for the IR receiver. Should be possible with e.g. the
  // TSOP31238 IR receiver as it typically uses only 0.35 mA.
  #define IR_VCC D5
  #define IR_GND D0

#endif // IR_TSOP312XX

#ifdef ON_DESK_MFD_ESP_MAC

  // Used only by test setup on desk
  #define TEST_IR_RECV_PIN D7
  #define TEST_IR_VCC_TEST D5
  #define TEST_IR_GND D0

#endif // ON_DESK_MFD_ESP_MAC

// -----
// Debugging

// Define to see infrared key hash values and timing on the serial port.
//#define DEBUG_IR_RECV

//#define DEBUG_WEBSERVER
//#define DEBUG_WEBSOCKET 1  // Set to 2 for more output
//#define DEBUG_ORIGINAL_MFD

// When set to 1, sends small test packets (~ 55 bytes). When set to 2, sends large test packets (~ 2.5 KByte).
//#define WIFI_STRESS_TEST 1

// Define to show the VAN bus statistics on the "system" screen
// Requires installation of the 'PrintEx' library; see: https://github.com/Chris--A/PrintEx (tested with version 1.2.0)
// Note that the 'PrintEx' library only compiles under ESP8266 Arduino release 2.7.4 or lower.
// To use the 'PrintEx' library with ESP8266 Arduino release 3.0.0 or higher, a patch is needed in the file
// c:\Users\<user_id>\Documents\Arduino\libraries\PrintEx\src\lib\TypeTraits.h . Edit that file; replace
// "struct select" by "struct select_P".
//#define SHOW_VAN_RX_STATS

// Define to show the ESP RAM and Wi-Fi statistics on the "system" screen
#define SHOW_ESP_RUNTIME_STATS

// Prints each packet on serial port, highlighting the bytes that differ from the previous
// packet with the same IDEN value
//#define PRINT_RAW_PACKET_DATA

// Define to see JSON buffers printed on the serial port
//#define PRINT_JSON_BUFFERS_ON_SERIAL

// If PRINT_JSON_BUFFERS_ON_SERIAL or PRINT_RAW_PACKET_DATA is defined, which type of VAN-bus packets will be
// printed on the serial port?
#define SELECTED_PACKETS VAN_PACKETS_ALL_VAN_PKTS
//#define SELECTED_PACKETS VAN_PACKETS_COM2000_ETC_PKTS
//#define SELECTED_PACKETS VAN_PACKETS_HEAD_UNIT_PKTS
//#define SELECTED_PACKETS VAN_PACKETS_SAT_NAV_PKTS
//#define SELECTED_PACKETS VAN_PACKETS_NO_VAN_PKTS

//#define PRINT_VAN_CRC_ERROR_PACKETS_ON_SERIAL

// Define to prepend a time stamp to debug output.
// When not defined, output looks like this:
//   Starting VAN bus "Live Connect" server
// When defined, output looks like this:
//   [08:43:45.641] Starting VAN bus "Live Connect" server
//
// This requires the Arduino 'Time' library (Paul Stoffregen, https://github.com/PaulStoffregen/Time,
// https://playground.arduino.cc/Code/Time/ ). To install this library using the Arduino IDE, choose
// menu "Sketch" --> "Include Library" --> "Manage Libraries", then enter "timelib" in the search box.
// Click on the 'Time' entry, select the latest version (currently 1.6.1), then click the 'Install' button.
//
//#define PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

#endif // Config_h
