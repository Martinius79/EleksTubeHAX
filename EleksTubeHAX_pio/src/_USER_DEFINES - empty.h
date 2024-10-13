/*
 * Project: Alternative firmware for EleksTube IPS clock
 * Hardware: ESP32
 * File description: User preferences for the complete project
 * Hardware connections and other deep settings are located in "GLOBAL_DEFINES.h"
 */


#ifndef USER_DEFINES_H_
#define USER_DEFINES_H_


//#define DEBUG_OUTPUT


// ************* Type of the clock hardware  *************
#define HARDWARE_Elekstube_CLOCK            // uncomment for the original Elekstube clock
//#define HARDWARE_Elekstube_CLOCK_Gen2     // uncomment for the original Elekstube clock Gen2.1 (ESP32 Pico D4 Chip)
//#define HARDWARE_SI_HAI_CLOCK             // uncomment for the SI HAI copy of the clock
//#define HARDWARE_NovelLife_SE_CLOCK       // uncomment for the NovelLife SE version (Gesture only) - tested and working!; Non-SE version (Buttons only) NOT tested!; Pro version (Buttons and Gesture) NOT tested!
//#define HARDWARE_PunkCyber_CLOCK          // uncomment for the PunkCyber / RGB Glow tube / PCBway clock
//#define HARDWARE_IPSTUBE_CLOCK            // uncomment for the IPSTUBE clock models (H401 and H402)


// ************* Clock font file type selection (.clk or .bmp)  *************
//#define USE_CLK_FILES   // select between .CLK and .BMP images


// ************* Display Dimming / Night time operation *************
#define NIGHTTIME_DIMMING               // uncomment to enable dimming in the given time period between NIGHT_TIME and DAY_TIME
#define NIGHT_TIME                  22  // dim displays at 10 pm 
#define DAY_TIME                    7   // full brightness after 7 am
#define BACKLIGHT_DIMMED_INTENSITY  1   // 0..7
#define TFT_DIMMED_INTENSITY        20  // 0..255

#ifdef HARDWARE_IPSTUBE_CLOCK
  // comment the next line out, to disable hardware dimming with GPIO4 pin (TFT_ENABLE_PIN) for IPSTUBE clock
  // (in case you have a clock that does not support hardware dimming because of missing Q1 transistor)
  #define DIM_WITH_ENABLE_PIN_PWM
  // it is recommended to try this feature only if you are sure that your clock supports hardware dimming
  // (that is, it is available in native application and really dims the backlight) because it may damage clock
  // if used with unsupported hardware! this feature only is expected to be supported safely by IPSTUBE clocks by now
  
  //NOTE: If NIGTHTIME_DIMMING is enabled: 
  // For the main LCDs: The dimming will be set to the hard coded value TFT_DIMMED_INTENSITY in the given time period EVERY HOUR beginning at NIGHT_TIME
  //    and will set back to the maximum brightness at DAY_TIME...Disable NIGHTTIME_DIMMING if you want to use the manual set dimming value all the time
  // For the backlight dimming: The dimming will ALWAYS stay to the hard coded value BACKLIGHT_DIMMED_INTENSITY in the given night time period! 
  //    The check for it is done and the value is apply every loop...Disable NIGHTTIME_DIMMING if you want to use the manual set dimming value all the time

  // TODO: Store the dimming values and dimming times in the NVS partition to keep the last dimming value and not use the hard coded values
  // make the times and values adjustable in the menu and/or via MQTT for both main and backlight dimming
#endif

#ifdef DIM_WITH_ENABLE_PIN_PWM
  // skip reinitialization if using PWM to control backlight since actual TFTs are never deactivated
  // disable by commenting the following line if this causes wake up issues from disabled (brightness 0) state
  #define TFT_SKIP_REINIT
#endif

// ************* WiFi config *************
#define WIFI_CONNECT_TIMEOUT_SEC    20
#define WIFI_RETRY_CONNECTION_SEC   15
#define WIFI_USE_WPS                                        //uncomment to use WPS instead of hard coded wifi credentials 
#define WIFI_SSID      "__enter_your_wifi_ssid_here__"      // not needed if WPS is used
#define WIFI_PASSWD    "__enter_your_wifi_password_here__"  // not needed if WPS is used.  Caution - Hard coded password is stored as clear text in BIN file


//  *************  Geolocation  *************
// Get your API Key on https://www.abstractapi.com/ (login) --> https://app.abstractapi.com/api/ip-geolocation/tester (key) *************
//#define GEOLOCATION_ENABLED    // enable after creating an account and copying Geolocation API below:
#define GEOLOCATION_API_KEY "__enter_your_api_key_here__" 


// ************* MQTT config *************
//#define MQTT_ENABLED  // enable after creating an account, setting up the Thermostat device on www.smartnest.cz and filling in all the data below:
//#define MQTT_HOME_ASSISTANT // enable if you want Home Assistant support
//#define MQTT_HOME_ASSISTANT_DISCOVERY
//#define MQTT_HOME_ASSISTANT_DISCOVERY_DEVICE_MANUFACTURER "EleksMaker"
//#define MQTT_HOME_ASSISTANT_DISCOVERY_DEVICE_MODEL "Elekstube IPS PR2"
//#define MQTT_HOME_ASSISTANT_DISCOVERY_SW_VERSION "0.8 Home Assistant Edition"
//#define MQTT_HOME_ASSISTANT_DISCOVERY_HW_VERSION "2.3.04"
#define MQTT_BROKER "smartnest.cz"             // Broker host
#define MQTT_PORT 1883                         // Broker port
#define MQTT_USERNAME "__enter_your_username_here__"             // Username from Smartnest
#define MQTT_PASSWORD "__enter_your_api_key_here__"              // Password from Smartnest or API key (under MY Account)
#define MQTT_CLIENT "__enter_your_device_id_here__"              // Device Id from Smartnest
#define MQTT_SAVE_PREFERENCES_AFTER_SEC 60

// ************* Optional temperature sensor *************
//#define ONE_WIRE_BUS_PIN   4  // DS18B20 connected to GPIO4; comment this line if sensor is not connected


#endif  // USER_DEFINES_H_
