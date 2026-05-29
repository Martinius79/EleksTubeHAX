#ifndef WIFI_WPS_H
#define WIFI_WPS_H

#include "GLOBAL_DEFINES.h"

enum WifiState_t
{
    disconnected,
    connected,
    wps_active,
    wps_success,
    wps_failed,
    ap_portal_active,
    num_states
};
void WifiBegin();
void WiFiStartWps();
void WifiReconnect();

/// Start captive portal AP mode for WiFi provisioning.
/// Returns true if user configured WiFi and connected successfully.
bool WifiStartCaptivePortal();

/// Call in loop() when in AP portal mode to handle DNS and web clients.
void WifiPortalHandle();

/// Returns true if captive portal is currently active (AP mode).
bool WifiIsPortalActive();

extern WifiState_t WifiState;

#endif // WIFI_WPS_H
