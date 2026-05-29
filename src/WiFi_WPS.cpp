#include <Arduino.h>
#include <esp_wps.h>
#include <WiFi.h>
#include "StoredConfig.h"
#include "TFTs.h"
#include "WiFi_WPS.h"

extern StoredConfig stored_config;
extern char UniqueDeviceName[32];

WifiState_t WifiState = disconnected;

uint32_t TimeOfWifiReconnectAttempt = 0;
uint32_t WifiReconnectIntervalMs = WIFI_RETRY_CONNECTION_SEC * 1000UL;
const uint32_t WifiReconnectIntervalMaxMs = 60000UL;
bool WifiReconnectInProgress = false;
bool WifiWpsActive = false;

#ifdef WIFI_USE_WPS // WPS code

static esp_wps_config_t wps_config = WPS_CONFIG_INIT_DEFAULT(ESP_WPS_MODE); // Init with defaults

static void copy_wps_field(char *dest, size_t dest_size, const char *src)
{
  if (dest_size == 0)
    return;
  if (src == nullptr)
  {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, dest_size - 1);
  dest[dest_size - 1] = '\0';
}

// Set alternative WPS config.
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WPS/WPS.ino
void wpsInitConfig()
{
  memset(&wps_config, 0, sizeof(esp_wps_config_t));
  wps_config.wps_type = ESP_WPS_MODE;
  copy_wps_field(wps_config.factory_info.manufacturer, sizeof(wps_config.factory_info.manufacturer), ESP_MANUFACTURER);
  copy_wps_field(wps_config.factory_info.model_number, sizeof(wps_config.factory_info.model_number), ESP_MODEL_NUMBER);
  copy_wps_field(wps_config.factory_info.model_name, sizeof(wps_config.factory_info.model_name), ESP_MODEL_NAME);
  copy_wps_field(wps_config.factory_info.device_name, sizeof(wps_config.factory_info.device_name), UniqueDeviceName);
}
#endif

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
  switch (event)
  {
  case ARDUINO_EVENT_WIFI_STA_START:
    WifiState = disconnected;
    Serial.println("Station Mode Started");
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED: // IP not yet assigned
    Serial.println("Connected to AP: " + String(WiFi.SSID()));
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    Serial.print("Got IP: ");
    Serial.println(WiFi.localIP());
    WifiState = connected;
    WifiReconnectIntervalMs = WIFI_RETRY_CONNECTION_SEC * 1000UL;
    WifiReconnectInProgress = false;
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    WifiState = disconnected;
    if (!WifiWpsActive)
    {
      Serial.print("WiFi lost connection. Reason: ");
      Serial.println(info.wifi_sta_disconnected.reason);
    }
    WifiReconnect();
    break;
#ifdef WIFI_USE_WPS // WPS code
  case ARDUINO_EVENT_WPS_ER_SUCCESS:
    if (!WifiWpsActive)
      break;
    WifiState = wps_success;
    Serial.println("WPS Successful, stopping WPS and connecting to: " + String(WiFi.SSID()));
    esp_wifi_wps_disable();
    delay(10);
    WiFi.begin();
    break;
  case ARDUINO_EVENT_WPS_ER_FAILED:
    if (!WifiWpsActive)
      break;
    WifiState = wps_failed;
    Serial.println("WPS Failed, retrying");
    esp_wifi_wps_disable();
    wpsInitConfig();
    esp_wifi_wps_enable(&wps_config);
    esp_wifi_wps_start(0);
    break;
  case ARDUINO_EVENT_WPS_ER_TIMEOUT:
    if (!WifiWpsActive)
      break;
    Serial.println("WPS Timeout, retrying");
    tfts.setTextColor(TFT_RED, TFT_BLACK);
    tfts.print("/"); // retry
    tfts.setTextColor(TFT_BLUE, TFT_BLACK);
    esp_wifi_wps_disable();
    wpsInitConfig();
    esp_wifi_wps_enable(&wps_config);
    esp_wifi_wps_start(0);
    WifiState = wps_active;
    break;
#endif
  default:
    break;
  }
}

void WifiBegin()
{
  WifiState = disconnected;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false); // we do our own reconnection handling!
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(UniqueDeviceName); // Set the hostname for DHCP

#ifdef WIFI_USE_WPS
  // no data is saved, start WPS imediatelly
  if (stored_config.config.wifi.WPS_connected != StoredConfig::valid)
  {
    // Config is invalid, probably a new device never had its config written.
    Serial.println("Loaded Wifi config is invalid. Not connecting to WiFi.");
    WiFiStartWps(); // infinite loop until connected
  }
  else
  {
    // Data is saved, connect now.
    // WiFi credentials are known, connect.
    tfts.println("Joining WiFi");
    tfts.println(stored_config.config.wifi.ssid);
    Serial.print("Joining WiFi ");
    Serial.println(stored_config.config.wifi.ssid);

    // https://stackoverflow.com/questions/48024780/esp32-wps-reconnect-on-power-on
    WiFi.begin(); // Use internally-saved data
    WiFi.onEvent(WiFiEvent);

    unsigned long StartTime = millis();

    while ((WiFi.status() != WL_CONNECTED))
    {
      delay(500);
      tfts.print(".");
      Serial.print(".");
      if ((millis() - StartTime) > (WIFI_CONNECT_TIMEOUT_SEC * 1000))
      {
        Serial.println("\r\nWiFi connection timeout!");
        tfts.println("\nTIMEOUT!");
        WifiState = disconnected;
        return; // exit loop, exit procedure, continue clock startup
      }
    }
  }
#else // NO WPS -- Use stored credentials, hardcoded fallback, or captive portal
  // Determine which credentials to use
  const char *useSSID = nullptr;
  const char *usePass = nullptr;

  if (stored_config.config.wifi.WPS_connected == StoredConfig::valid &&
      strlen(stored_config.config.wifi.ssid) > 0)
  {
    // Use stored credentials (previously configured via portal or WPS)
    useSSID = stored_config.config.wifi.ssid;
    usePass = stored_config.config.wifi.password;
    Serial.printf("Using stored WiFi credentials for SSID: '%s'\n", useSSID);
  }
  else
  {
#if defined(WIFI_SSID) && defined(WIFI_PASSWD)
    // Fallback to hardcoded credentials
    useSSID = WIFI_SSID;
    usePass = WIFI_PASSWD;
    Serial.printf("Using hardcoded WiFi credentials for SSID: '%s'\n", useSSID);
#else
    // No credentials available at all - go straight to captive portal
    Serial.println("No WiFi credentials available. Starting captive portal...");
    tfts.setTextColor(TFT_YELLOW, TFT_BLACK);
    tfts.println("No WiFi config!");
    tfts.println("Starting AP...");
    WifiStartCaptivePortal();
    return;
#endif
  }

  WiFi.begin(useSSID, usePass);
  WiFi.onEvent(WiFiEvent);
  unsigned long StartTime = millis();
  while ((WiFi.status() != WL_CONNECTED))
  {
    delay(500);
    tfts.print(">");
    Serial.print(">");
    if ((millis() - StartTime) > (WIFI_CONNECT_TIMEOUT_SEC * 1000))
    {
      tfts.setTextColor(TFT_RED, TFT_BLACK);
      tfts.println("\nTIMEOUT!");
      tfts.setTextColor(TFT_YELLOW, TFT_BLACK);
      tfts.println("Starting AP...");
      Serial.println("\r\nWiFi connection timeout! Starting captive portal...");
      WifiStartCaptivePortal();
      return;
    }
  }

#endif

  if (WifiState == connected)
  {
    tfts.println("\nConnected! IP:");
    tfts.println(WiFi.localIP());
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    delay(200);
  }
  else
  {
    Serial.println("Connecting to WiFi failed! No WiFi! Clock will not show actual time or last saved time!");
  }
}

void WifiReconnect()
{
#ifdef WIFI_USE_WPS
  static bool warnedNoCreds = false;
  if (WifiWpsActive)
  {
    return;
  }
  if (stored_config.config.wifi.WPS_connected != StoredConfig::valid)
  {
    if (!warnedNoCreds)
    {
      Serial.println("WiFi reconnect skipped: no stored credentials (WPS not completed).");
      warnedNoCreds = true;
    }
    return;
  }
#endif
  if (WifiReconnectInProgress)
  {
    if ((millis() - TimeOfWifiReconnectAttempt) < 5000UL)
    {
      return;
    }
    WifiReconnectInProgress = false; // allow a new attempt after timeout
  }

  if ((WifiState == disconnected) && ((millis() - TimeOfWifiReconnectAttempt) > WifiReconnectIntervalMs))
  {
    Serial.println("Attempting WiFi reconnection...");
    WifiReconnectInProgress = true;
    WiFi.disconnect(false, false);
    delay(200);
    WiFi.begin();
    TimeOfWifiReconnectAttempt = millis();
    WifiReconnectIntervalMs = min(WifiReconnectIntervalMs * 2, WifiReconnectIntervalMaxMs);
  }
}

#ifdef WIFI_USE_WPS // WPS code
void WiFiStartWps()
{
  const uint32_t WPS_RESTART_INTERVAL_MS = 30000;                           // Restart WPS every 30s to catch late router activation
  const uint8_t WPS_HARD_RESET_EVERY = 3;                                   // Full WiFi reset every 3 restarts
  memset(&stored_config.config.wifi, 0, sizeof(stored_config.config.wifi)); // erase all settings
  stored_config.config.wifi.password[0] = '\0';                             // empty string as password
  stored_config.config.wifi.WPS_connected = 0x11;                           // invalid = different than 0x55
  Serial.println("");
  Serial.print("Saving config! Triggered from WPS start (erasing)...");
  stored_config.save();
  Serial.println(" Done.");

  tfts.setTextColor(TFT_GREEN, TFT_BLACK);
  tfts.println("WPS STARTED!");
  tfts.setTextColor(TFT_RED, TFT_BLACK);
  tfts.println("PRESS WPS BUTTON ON THE ROUTER");

  // Disconnect from wifi first if we were connected.
  WiFi.disconnect(true, true);

  WifiState = wps_active;
  WifiWpsActive = true;
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_MODE_STA);

  Serial.println("Starting WPS");

  wpsInitConfig();
  esp_wifi_wps_enable(&wps_config);
  esp_wifi_wps_start(0);
  uint32_t lastWpsStartMs = millis();
  uint32_t wpsStartMs = millis();
  uint8_t wpsRestartCount = 0;

  // Loop until connected.
  tfts.setTextColor(TFT_BLUE, TFT_BLACK);
  while (WifiState != connected)
  {
    delay(2000);
    tfts.print(".");
    Serial.print(".");
    if ((millis() - wpsStartMs) > (WIFI_WPS_CONNECT_TIMEOUT_SEC * 1000UL))
    {
      tfts.setTextColor(TFT_RED, TFT_BLACK);
      tfts.println("WPS FAILED! NO WiFi");
      tfts.setTextColor(TFT_WHITE, TFT_BLACK);
      Serial.println("\r\nWPS FAILED! Going on without WiFi.");
      WifiState = disconnected;
      break;
    }
    if ((millis() - lastWpsStartMs) > WPS_RESTART_INTERVAL_MS)
    {
      wpsRestartCount++;
      Serial.println("\r\nWPS restart to catch late router activation");
      tfts.setTextColor(TFT_ORANGE, TFT_BLACK);
      tfts.print("/");
      tfts.setTextColor(TFT_BLUE, TFT_BLACK);
      if ((wpsRestartCount % WPS_HARD_RESET_EVERY) == 0)
      {
        Serial.println("WPS full WiFi reset");
        tfts.setTextColor(TFT_YELLOW, TFT_BLACK);
        tfts.print("/");
        tfts.setTextColor(TFT_BLUE, TFT_BLACK);
        WiFi.disconnect(true, true);
        delay(200);
        WiFi.mode(WIFI_MODE_STA);
        delay(200);
      }
      esp_wifi_wps_disable();
      wpsInitConfig();
      esp_wifi_wps_enable(&wps_config);
      esp_wifi_wps_start(0);
      WifiState = wps_active;
      lastWpsStartMs = millis();
    }
  }
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  if (WifiState != connected)
  {
    esp_wifi_wps_disable();
    WifiWpsActive = false;
    Serial.println("WPS finished without success! No credentials saved.");
    return;
  }
  snprintf(stored_config.config.wifi.ssid, sizeof(stored_config.config.wifi.ssid), "%s", WiFi.SSID().c_str()); // Copy the SSID into the stored configuration safely
  memset(&stored_config.config.wifi.password, 0, sizeof(stored_config.config.wifi.password));                  // Since the password cannot be retrieved from WPS, overwrite it completely
  stored_config.config.wifi.password[0] = '\0';                                                                // ...and set to an empty string
  stored_config.config.wifi.WPS_connected = StoredConfig::valid;                                               // Mark the configuration as valid
  Serial.println();
  Serial.print("Saving config! Triggered from WPS success (saving)...");
  stored_config.save();
  Serial.println(" WPS finished.");
  WifiWpsActive = false;
}
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Captive Portal for WiFi Provisioning
// ═══════════════════════════════════════════════════════════════════════════════

#include <WebServer.h>
#include <DNSServer.h>

static WebServer *portalServer = nullptr;
static DNSServer *dnsServer = nullptr;
static bool portalActive = false;
static String scannedNetworks = "";

static const char PORTAL_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>EleksTubeHAX WiFi Setup</title>
<style>
  body { font-family: -apple-system, sans-serif; max-width: 400px; margin: 0 auto; padding: 20px; background: #1a1a2e; color: #eee; }
  h1 { color: #e94560; text-align: center; font-size: 1.4em; }
  .info { background: #16213e; border-radius: 8px; padding: 12px; margin: 10px 0; font-size: 0.9em; color: #aaa; }
  label { display: block; margin: 12px 0 4px; font-size: 0.9em; color: #ccc; }
  input, select { width: 100%; padding: 10px; border: 1px solid #333; border-radius: 4px; background: #0f3460; color: #eee; box-sizing: border-box; font-size: 1em; }
  button { width: 100%; padding: 12px; margin-top: 20px; border: none; border-radius: 4px; background: #e94560; color: white; font-weight: bold; font-size: 1.1em; cursor: pointer; }
  button:hover { background: #c73e54; }
  .networks { max-height: 200px; overflow-y: auto; }
  .net-item { padding: 8px; cursor: pointer; border-bottom: 1px solid #333; }
  .net-item:hover { background: #0f3460; }
  .signal { float: right; color: #888; font-size: 0.8em; }
</style>
</head><body>
<h1>EleksTubeHAX WiFi Setup</h1>
<div class="info">Connect your clock to your home WiFi network.</div>
<form method="POST" action="/save">
  <label>Available Networks:</label>
  <div class="networks" id="nets">%NETWORKS%</div>
  <label>SSID:</label>
  <input type="text" name="ssid" id="ssid" maxlength="31" required>
  <label>Password:</label>
  <input type="password" name="pass" id="pass" maxlength="31">
  <button type="submit">Connect & Save</button>
</form>
<script>
document.querySelectorAll('.net-item').forEach(el => {
  el.addEventListener('click', () => { document.getElementById('ssid').value = el.dataset.ssid; });
});
</script>
</body></html>)rawliteral";

static const char PORTAL_SUCCESS[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WiFi Configured</title>
<style>body{font-family:sans-serif;max-width:400px;margin:0 auto;padding:20px;background:#1a1a2e;color:#eee;text-align:center;}
h1{color:#0f3;}p{color:#aaa;}</style>
</head><body><h1>WiFi Configured!</h1><p>The clock will now connect to your network and restart.</p>
<p>You can close this page.</p></body></html>)rawliteral";

static void scanNetworks()
{
  Serial.println("Portal: scanning WiFi networks...");
  int n = WiFi.scanNetworks();
  scannedNetworks = "";
  for (int i = 0; i < n && i < 15; i++)
  {
    int rssi = WiFi.RSSI(i);
    const char *enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "🔒";
    scannedNetworks += "<div class='net-item' data-ssid='" + WiFi.SSID(i) + "'>";
    scannedNetworks += WiFi.SSID(i);
    scannedNetworks += "<span class='signal'>" + String(rssi) + " dBm " + enc + "</span></div>";
  }
  WiFi.scanDelete();
  if (n == 0)
    scannedNetworks = "<div class='net-item'>No networks found</div>";
}

static void handlePortalRoot()
{
  String page = String(PORTAL_HTML);
  page.replace("%NETWORKS%", scannedNetworks);
  portalServer->send(200, "text/html", page);
}

static void handlePortalSave()
{
  if (!portalServer->hasArg("ssid") || portalServer->arg("ssid").length() == 0)
  {
    portalServer->send(400, "text/plain", "SSID is required");
    return;
  }

  String ssid = portalServer->arg("ssid");
  String pass = portalServer->hasArg("pass") ? portalServer->arg("pass") : "";

  // Save to stored_config
  snprintf(stored_config.config.wifi.ssid, sizeof(stored_config.config.wifi.ssid), "%s", ssid.c_str());
  snprintf(stored_config.config.wifi.password, sizeof(stored_config.config.wifi.password), "%s", pass.c_str());
  stored_config.config.wifi.WPS_connected = StoredConfig::valid;

  Serial.printf("Portal: saving WiFi credentials for SSID '%s'\n", ssid.c_str());
  stored_config.save();

  portalServer->send(200, "text/html", String(PORTAL_SUCCESS));
  Serial.println("Portal: credentials saved, restarting in 2s...");

  delay(2000);
  ESP.restart();
}

static void handlePortalNotFound()
{
  // Captive portal: redirect all requests to the config page
  portalServer->sendHeader("Location", "http://192.168.4.1/", true);
  portalServer->send(302, "text/plain", "");
}

bool WifiStartCaptivePortal()
{
  Serial.println("Starting WiFi Captive Portal (AP mode)...");

  // Stop any existing STA connection
  WiFi.disconnect(true, false);
  delay(100);

  // Create AP
  String apName = String("EleksTubeHAX-") + String(UniqueDeviceName).substring(String(UniqueDeviceName).lastIndexOf('-') + 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str());
  delay(100);

  Serial.printf("Portal AP started: '%s', IP: %s\n", apName.c_str(), WiFi.softAPIP().toString().c_str());

  // Scan for available networks
  WiFi.mode(WIFI_AP_STA);
  scanNetworks();
  WiFi.mode(WIFI_AP);

  // Start DNS server (captive portal redirect)
  dnsServer = new DNSServer();
  dnsServer->start(53, "*", WiFi.softAPIP());

  // Start web server
  portalServer = new WebServer(80);
  portalServer->on("/", HTTP_GET, handlePortalRoot);
  portalServer->on("/save", HTTP_POST, handlePortalSave);
  portalServer->onNotFound(handlePortalNotFound);
  portalServer->begin();

  portalActive = true;
  WifiState = ap_portal_active;

  return true;
}

void WifiPortalHandle()
{
  if (!portalActive)
    return;
  dnsServer->processNextRequest();
  portalServer->handleClient();
}

bool WifiIsPortalActive()
{
  return portalActive;
}
