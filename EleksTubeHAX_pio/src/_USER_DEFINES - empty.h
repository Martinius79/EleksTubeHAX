/*
 * Project: Alternative firmware for EleksTube IPS clock
 * Hardware: ESP32
 * File description: User preferences for the complete project
 * Hardware connections and other deep settings are located in "GLOBAL_DEFINES.h"
 */

#ifndef USER_DEFINES_H_
#define USER_DEFINES_H_

// *************  Debug output  *************
#define DEBUG_OUTPUT // uncomment for Debug logs via serial interface

// --- extended debug output ---
// #define DEBUG_OUTPUT_TFT                 // uncomment for TFT class debug output
// #define DEBUG_OUTPUT_VERBOSE                // uncomment for verbose debug output
// #define DEBUG_OUTPUT_MENU                // uncomment for menu specific debug output
// #define DEBUG_OUTPUT_BUTTONS             // uncomment for buttons specific debug output
// #define DEBUG_OUTPUT_MQTT                // uncomment for MQTT specific debug output
// #define DEBUG_OUTPUT_BACKLIGHTS          // uncomment for backlight specific debug output
// #define DEBUG_OUTPUT_CHIPSELECT          // uncomment for chip select specific debug output
// #define DEBUG_OUTPUT_NTP                 // uncomment for NTP specific debug output

// ************* Type of the clock hardware  *************
#define HARDWARE_Elekstube_CLOCK // uncomment for the original Elekstube clock
// #define HARDWARE_Elekstube_CLOCK_Gen2     // uncomment for the original Elekstube clock Gen2.1 (ESP32 Pico D4 Chip)
// #define HARDWARE_SI_HAI_CLOCK             // uncomment for the SI HAI copy of the clock
// #define HARDWARE_NovelLife_SE_CLOCK       // uncomment for the NovelLife SE version (Gesture only) - tested and working!; Non-SE version (Buttons only) NOT tested!; Pro version (Buttons and Gesture) NOT tested!
// #define HARDWARE_PunkCyber_CLOCK          // uncomment for the PunkCyber / RGB Glow tube / PCBway clock
// #define HARDWARE_IPSTUBE_CLOCK            // uncomment for the IPSTUBE clock models (H401 and H402)

// ************* For IPSTUBE clocks only *************
// Unomment the next line, to DISABLE hardware dimming with GPIO4 pin (TFT_ENABLE_PIN) for a IPSTUBE clock
// See the IPSTUBE clock hardware section in GLOBAL_DEFINES.h for more information!
// #undef DIM_WITH_ENABLE_PIN_PWM

// ************* Clock font file type selection (.clk or .bmp)  *************
// #define USE_CLK_FILES   // select between .CLK and .BMP images

// ************* Display Dimming / Night time operation *************
#define NIGHTTIME_DIMMING               // uncomment to enable dimming in the given time period between NIGHT_TIME_START and DAY_TIME_START
#define NIGHT_TIME_START 22             // dim displays at 10 pm
#define DAY_TIME_START 7                // full brightness after 7 am
#define BACKLIGHT_NIGHTTIME_INTENSITY 1 // 0..7
#define BACKLIGHT_DAYTIME_INTENSITY 7   // 0..7
#define TFT_NIGHTTIME_INTENSITY 20      // 0..255
#define TFT_DAYTIME_INTENSITY 255       // 0..255

// ************* WiFi config *************
#define WIFI_CONNECT_TIMEOUT_SEC 20
#define WIFI_RETRY_CONNECTION_SEC 15
#define WIFI_USE_WPS                                    // uncomment to use WPS instead of hard coded wifi credentials
#define WIFI_SSID "__enter_your_wifi_ssid_here__"       // not needed if WPS is used
#define WIFI_PASSWD "__enter_your_wifi_password_here__" // not needed if WPS is used.  Caution - Hard coded password is stored as clear text in BIN file

//  *************  Geolocation  *************
// Get your API Key on https://www.abstractapi.com/ (login) --> https://app.abstractapi.com/api/ip-geolocation/tester (key) *************
// #define GEOLOCATION_ENABLED    // enable after creating an account and copying Geolocation API below:
#define GEOLOCATION_API_KEY "__enter_your_api_key_here__"

// *************  MQTT config  *************
#define MQTT_ENABLED // Uncomment if you want to enable MQTT support at all (settings see below)

// --- MQTT - Just for naming ---
#if defined(HARDWARE_Elekstube_CLOCK)
#define DEVICE_MANUFACTURER "EleksMaker"
#define MQTT_SUFFIX "_Elekstube"
#define HARDWARE_VERSION "1.x"
#elif defined(HARDWARE_Elekstube_CLOCK_Gen2)
#define DEVICE_MANUFACTURER "EleksMaker"
#define MQTT_SUFFIX "_ElekstubeGen2"
#define HARDWARE_VERSION "2.x"
#elif defined(HARDWARE_SI_HAI_CLOCK)
#define DEVICE_MANUFACTURER "SI HAI"
#define MQTT_SUFFIX "_SI_HAI"
#define HARDWARE_VERSION "1.x"
#elif defined(HARDWARE_NovelLife_SE_CLOCK)
#define DEVICE_MANUFACTURER "NovelLife"
#define MQTT_SUFFIX "_NovelLifeSE"
#define HARDWARE_VERSION "1.x"
#elif defined(HARDWARE_PunkCyber_CLOCK)
#define DEVICE_MANUFACTURER "PunkCyber"
#define MQTT_SUFFIX "_PunkCyber"
#define HARDWARE_VERSION "1.x"
#elif defined(HARDWARE_IPSTUBE_CLOCK)
#define DEVICE_MANUFACTURER "IPSTUBE"
#define MQTT_SUFFIX "_IPSTUBE"
#define HARDWARE_VERSION "1.x"
#else
#define DEVICE_MANUFACTURER = "Unknown"
#define MQTT_SUFFIX "_Unknown"
#define HARDWARE_VERSION "x.x"
#endif
#define MQTT_BASE_NAME "EleksTubeHAXClock" // Base name of the MQTT topics

// --- MQTT Home Assistant settings ---
#define MQTT_HOME_ASSISTANT                                                   // Uncomment if you want Home Assistant (HA) support
#define MQTT_HOME_ASSISTANT_DISCOVERY                                         // Uncomment if you want HA auto-discovery (requires MQTT_HOME_ASSISTANT)
#define MQTT_HOME_ASSISTANT_DISCOVERY_DEVICE_MANUFACTURER DEVICE_MANUFACTURER // Name of the manufacturer shown in HA
#define MQTT_HOME_ASSISTANT_DISCOVERY_DEVICE_MODEL MQTT_BASE_NAME MQTT_SUFFIX // Name of the model shown in HA
#define MQTT_HOME_ASSISTANT_DISCOVERY_SW_VERSION FIRMWARE_VERSION             // Firmware version shown in HA
#define MQTT_HOME_ASSISTANT_DISCOVERY_HW_VERSION HARDWARE_VERSION             // Hardware version shown in HA

// --- MQTT broker settings ---
// You can either use an internet-based MQTT broker (smartnest.cz) or a local one (e.g. Mosquitto).
// If you choose an internet based one, you will need to create an account, setting up the device there and filling in the data below then.
// If you choose a local one, you will need to set up the broker on your local network and fill in the data below.
#define MQTT_BROKER "smartnest.cz"                   // Broker host (or IP address) - can also be your local MQTT server!
#define MQTT_PORT 1883                               // Broker port
#define MQTT_USERNAME "__enter_your_username_here__" // Username (i.e. from Smartnest or your local MQTT server)
#define MQTT_PASSWORD "__enter_your_api_key_here__"  // Password (i.e. from Smartnest or the API key under "My Account" or from your local MQTT server)
#define MQTT_CLIENT MQTT_BASE_NAME MQTT_SUFFIX       // Device ID (i.e. from Smartnest) - This will be used as Root for each Topic - Local can be anything, like "EleksTubeHAXClock"
#define MQTT_SAVE_CONFIG_AFTER_SEC 60                // Save the preferences after X seconds after the last MQTT command was received

// ************* Optional temperature sensor *************
// #define ONE_WIRE_BUS_PIN   4  // DS18B20 connected to GPIO4; comment this line if sensor is not connected

#endif // USER_DEFINES_H_
