/*
 * User defines for the Wokwi simulator build (env EleksTube_Wokwi, see wokwi/README.md).
 * Only used when WOKWI_SIMULATION is defined. Takes the repository template as-is and
 * replaces the WiFi setup with the open "Wokwi-GUEST" network of the simulator.
 * No personal values belong here - the simulator reaches the internet through the Wokwi gateway.
 */

#pragma once

#include "_USER_DEFINES - empty.h"

// Debug output helps a lot in the simulator, the serial monitor is always available.
#ifndef DEBUG_OUTPUT
#define DEBUG_OUTPUT
#endif

// The simulated ESP32 has no WPS button partner - use fixed credentials of the simulator WiFi.
#undef WIFI_USE_WPS
#undef WIFI_SSID
#undef WIFI_PASSWD
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWD ""
