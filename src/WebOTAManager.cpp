#include "WebOTAManager.h"

void WebOTAManager::begin(Clock *uclock, TFTs *tfts, Backlights *backlights, StoredConfig *config, const char *deviceName)
{
  _uclock = uclock;
  _tfts = tfts;
  _backlights = backlights;
  _config = config;
  _deviceName = deviceName;

  if (!WiFi.isConnected())
  {
    Serial.println("WebOTA: WiFi not connected, skipping web server start.");
    return;
  }

  setupRoutes();

  // ElegantOTA attaches to the WebServer
  ElegantOTA.begin(&_server);

  _server.begin();
  _started = true;
  Serial.printf("Web server started at http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.println("OTA update available at http://<IP>/update");
}

void WebOTAManager::handle()
{
  if (!_started)
    return;
  _server.handleClient();
  ElegantOTA.loop();
}

void WebOTAManager::setupRoutes()
{
  _server.on("/", HTTP_GET, [this]()
             { _server.send(200, "text/html", buildMainPage()); });

  _server.on("/api/status", HTTP_GET, [this]()
             { _server.send(200, "application/json", buildStatusJson()); });

  _server.on("/api/command", HTTP_POST, [this]()
             { handleApiCommand(); });
}

String WebOTAManager::buildMainPage()
{
  String html = F(R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>EleksTubeHAX</title>
<style>
  body { font-family: -apple-system, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px; background: #1a1a2e; color: #eee; }
  h1 { color: #0f3460; background: #e94560; padding: 10px; border-radius: 8px; text-align: center; }
  .card { background: #16213e; border-radius: 8px; padding: 15px; margin: 10px 0; }
  .card h2 { margin-top: 0; color: #e94560; font-size: 1.1em; }
  label { display: block; margin: 8px 0 4px; font-size: 0.9em; color: #aaa; }
  input[type=range] { width: 100%; }
  select, button { padding: 8px 16px; border: none; border-radius: 4px; margin: 4px; cursor: pointer; }
  button { background: #e94560; color: white; font-weight: bold; }
  button:hover { background: #c73e54; }
  button.off { background: #555; }
  select { background: #0f3460; color: #eee; }
  .status { font-size: 0.85em; color: #888; }
  #time { font-size: 2em; text-align: center; font-family: monospace; color: #0f3; }
  .btn-group { display: flex; gap: 8px; flex-wrap: wrap; }
  a { color: #e94560; }
</style>
</head><body>
<h1>)rawliteral");

  html += _deviceName;
  html += F(R"rawliteral(</h1>
<div class="card"><div id="time">--:--:--</div></div>

<div class="card">
  <h2>Display</h2>
  <div class="btn-group">
    <button onclick="cmd('power','on')">Display ON</button>
    <button class="off" onclick="cmd('power','off')">Display OFF</button>
  </div>
  <label>Brightness: <span id="brVal">--</span></label>
  <input type="range" min="0" max="255" id="brightness" oninput="cmd('brightness',this.value);document.getElementById('brVal').innerText=this.value">
  <label>Clock Face: <span id="gfxVal">--</span></label>
  <input type="range" min="1" max="9" id="graphic" oninput="cmd('graphic',this.value);document.getElementById('gfxVal').innerText=this.value">
</div>

<div class="card">
  <h2>Backlight</h2>
  <div class="btn-group">
    <button onclick="cmd('blpower','on')">Backlight ON</button>
    <button class="off" onclick="cmd('blpower','off')">Backlight OFF</button>
  </div>
  <label>Intensity: <span id="blVal">--</span></label>
  <input type="range" min="0" max="7" id="blintensity" oninput="cmd('backlight',this.value);document.getElementById('blVal').innerText=this.value">
</div>

<div class="card">
  <h2>Settings</h2>
  <div class="btn-group">
    <button onclick="cmd('timeformat','12')">12h</button>
    <button onclick="cmd('timeformat','24')">24h</button>
    <button onclick="cmd('save','')">Save Config</button>
    <button class="off" onclick="if(confirm('Reboot?'))cmd('reboot','')">Reboot</button>
  </div>
</div>

<div class="card">
  <h2>Firmware Update</h2>
  <p><a href="/update">Open OTA Update Page</a></p>
</div>

<div class="card status" id="statusInfo">Loading...</div>

<script>
function cmd(c, v) {
  fetch('/api/command', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'cmd='+c+'&val='+v});
}
function refresh() {
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('time').innerText = d.time;
    document.getElementById('brVal').innerText = d.brightness;
    document.getElementById('brightness').value = d.brightness;
    document.getElementById('gfxVal').innerText = d.graphic;
    document.getElementById('graphic').value = d.graphic;
    document.getElementById('graphic').max = d.graphicMax;
    document.getElementById('blVal').innerText = d.blIntensity;
    document.getElementById('blintensity').value = d.blIntensity;
    document.getElementById('statusInfo').innerHTML =
      'WiFi: '+d.ssid+' ('+d.rssi+' dBm) | IP: '+d.ip+' | Heap: '+d.freeHeap+' | Uptime: '+d.uptime+'s | FW: v'+d.firmware;
  }).catch(()=>{});
}
refresh();
setInterval(refresh, 2000);
</script>
</body></html>)rawliteral");

  return html;
}

String WebOTAManager::buildStatusJson()
{
  char buf[512];
  snprintf(buf, sizeof(buf),
           "{\"time\":\"%02d:%02d:%02d\",\"date\":\"%04d-%02d-%02d\","
           "\"brightness\":%d,\"graphic\":%d,\"graphicMax\":%d,"
           "\"blIntensity\":%d,\"blPower\":%s,\"displayPower\":%s,"
           "\"timeFormat\":\"%s\",\"pattern\":\"%s\","
           "\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,"
           "\"freeHeap\":%d,\"uptime\":%lu,\"firmware\":\"%s\"}",
           _uclock->getHour24(), _uclock->getMinute(), _uclock->getSecond(),
           _uclock->getYear(), _uclock->getMonth(), _uclock->getDay(),
           _tfts->dimming, _uclock->getActiveGraphicIdx(), _tfts->NumberOfClockFaces,
           _backlights->getIntensity(),
           _backlights->getPower() ? "true" : "false",
           _tfts->isEnabled() ? "true" : "false",
           _uclock->getTwelveHour() ? "12h" : "24h",
           _backlights->getPatternStr().c_str(),
           WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI(),
           ESP.getFreeHeap(), millis() / 1000, FIRMWARE_VERSION);
  return String(buf);
}

void WebOTAManager::handleApiCommand()
{
  if (!_server.hasArg("cmd"))
  {
    _server.send(400, "text/plain", "Missing 'cmd' parameter");
    return;
  }

  String cmd = _server.arg("cmd");
  String val = _server.hasArg("val") ? _server.arg("val") : "";

  if (cmd == "brightness")
  {
    int v = val.toInt();
    if (v >= 0 && v <= 255)
    {
      _tfts->dimming = v;
      _tfts->ProcessUpdatedDimming();
    }
  }
  else if (cmd == "backlight")
  {
    int v = val.toInt();
    if (v >= 0 && v <= 7)
      _backlights->setIntensity(v);
  }
  else if (cmd == "graphic")
  {
    int v = val.toInt();
    if (v >= 1 && v <= _tfts->NumberOfClockFaces)
    {
      _uclock->setClockGraphicsIdx(v);
      _tfts->current_graphic = _uclock->getActiveGraphicIdx();
    }
  }
  else if (cmd == "power")
  {
    if (val == "on")
    {
      if (!_tfts->isEnabled())
      {
#ifdef HARDWARE_ELEKSTUBE_CLOCK
        _tfts->reinit();
#else
        _tfts->enableAllDisplays();
#endif
      }
    }
    else if (val == "off")
    {
      _tfts->chip_select.setAll();
      _tfts->fillScreen(TFT_BLACK);
      _tfts->disableAllDisplays();
    }
  }
  else if (cmd == "blpower")
  {
    if (val == "on")
      _backlights->PowerOn();
    else if (val == "off")
      _backlights->PowerOff();
  }
  else if (cmd == "timeformat")
  {
    if (val == "12")
      _uclock->setTwelveHour(true);
    else if (val == "24")
      _uclock->setTwelveHour(false);
  }
  else if (cmd == "save")
  {
    _config->save();
  }
  else if (cmd == "reboot")
  {
    _server.send(200, "text/plain", "Rebooting...");
    delay(100);
    ESP.restart();
    return;
  }

  _server.send(200, "text/plain", "OK");
}
