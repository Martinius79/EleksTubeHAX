#include "WebOTAManager.h"

void WebOTAManager::begin(Clock *uclock, TFTs *tfts, Backlights *backlights, StoredConfig *config,
                          DimmingManager *dimming, const char *deviceName)
{
  _uclock = uclock;
  _tfts = tfts;
  _backlights = backlights;
  _config = config;
  _dimming = dimming;
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

  _server.on("/wifi", HTTP_GET, [this]()
             { _server.send(200, "text/html", buildWifiPage()); });

  _server.on("/wifi/save", HTTP_POST, [this]()
             { handleWifiSave(); });

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
  body{font-family:-apple-system,sans-serif;max-width:600px;margin:0 auto;padding:20px;background:#1a1a2e;color:#eee}
  h1{color:#0f3460;background:#e94560;padding:10px;border-radius:8px;text-align:center}
  .card{background:#16213e;border-radius:8px;padding:15px;margin:10px 0}
  .card h2{margin-top:0;color:#e94560;font-size:1.1em}
  label{display:block;margin:8px 0 4px;font-size:0.9em;color:#aaa}
  input[type=range]{width:100%}
  input[type=number]{width:60px;padding:6px;border:1px solid #333;border-radius:4px;background:#0f3460;color:#eee}
  select,button{padding:8px 16px;border:none;border-radius:4px;margin:4px;cursor:pointer}
  button{background:#e94560;color:white;font-weight:bold}
  button:hover{background:#c73e54}
  button.off{background:#555}
  button.auto{background:#0a8}
  select{background:#0f3460;color:#eee}
  .status{font-size:0.85em;color:#888}
  #time{font-size:2em;text-align:center;font-family:monospace;color:#0f3}
  .btn-group{display:flex;gap:8px;flex-wrap:wrap}
  a{color:#e94560}
  .row{display:flex;align-items:center;gap:10px;margin:6px 0}
  .row label{margin:0;min-width:100px}
  .tag{display:inline-block;padding:2px 8px;border-radius:4px;font-size:0.8em;background:#333}
  .tag.on{background:#0a8;color:#fff}
  .tag.off{background:#555;color:#ccc}
  nav{text-align:center;margin:10px 0}
  nav a{margin:0 10px;font-size:0.95em}
</style>
</head><body>
<h1>)rawliteral");

  html += _deviceName;
  html += F(R"rawliteral(</h1>
<nav><a href="/">Home</a> | <a href="/wifi">WiFi</a> | <a href="/update">OTA Update</a></nav>

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
  <label>Pattern:</label>
  <select id="blpattern" onchange="cmd('pattern',this.value)">
    <option value="0">Dark</option>
    <option value="2">Constant</option>
    <option value="3">Rainbow</option>
    <option value="4">Pulse</option>
    <option value="5">Breath</option>
  </select>
</div>

<div class="card">
  <h2>Auto Dimming</h2>
  <div class="btn-group">
    <button class="auto" id="btnAutoOn" onclick="cmd('dim_auto','on')">Auto ON</button>
    <button class="off" id="btnAutoOff" onclick="cmd('dim_auto','off')">Auto OFF</button>
  </div>
  <span class="tag" id="dimMode">--</span>
  <div class="row"><label>Night start:</label><input type="number" min="0" max="23" id="nightH" onchange="cmd('dim_night_h',this.value)"> <span>:00</span></div>
  <div class="row"><label>Day start:</label><input type="number" min="0" max="23" id="dayH" onchange="cmd('dim_day_h',this.value)"> <span>:00</span></div>
  <div class="row"><label>TFT day:</label><input type="range" min="0" max="255" id="tftDay" onchange="cmd('dim_tft_day',this.value)"><span id="tftDayV">--</span></div>
  <div class="row"><label>TFT night:</label><input type="range" min="0" max="255" id="tftNight" onchange="cmd('dim_tft_night',this.value)"><span id="tftNightV">--</span></div>
  <div class="row"><label>BL day:</label><input type="range" min="0" max="7" id="blDay" onchange="cmd('dim_bl_day',this.value)"><span id="blDayV">--</span></div>
  <div class="row"><label>BL night:</label><input type="range" min="0" max="7" id="blNight" onchange="cmd('dim_bl_night',this.value)"><span id="blNightV">--</span></div>
</div>

<div class="card">
  <h2>Time Settings</h2>
  <div class="btn-group">
    <button onclick="cmd('timeformat','12')">12h</button>
    <button onclick="cmd('timeformat','24')">24h</button>
  </div>
  <div class="row"><label>UTC Offset (h):</label><input type="number" min="-12" max="14" step="0.25" id="tzOffset" onchange="cmd('timezone',this.value)"></div>
</div>

<div class="card">
  <h2>System</h2>
  <div class="btn-group">
    <button onclick="cmd('save','')">Save Config</button>
    <button class="off" onclick="if(confirm('Reboot?'))cmd('reboot','')">Reboot</button>
  </div>
</div>

<div class="card status" id="statusInfo">Loading...</div>

<script>
function cmd(c,v){fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'cmd='+c+'&val='+v}).then(()=>{if(c.startsWith('dim_'))setTimeout(refresh,300)})}
function refresh(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('time').innerText=d.time;
    document.getElementById('brVal').innerText=d.brightness;
    document.getElementById('brightness').value=d.brightness;
    document.getElementById('gfxVal').innerText=d.graphic;
    document.getElementById('graphic').value=d.graphic;
    document.getElementById('graphic').max=d.graphicMax;
    document.getElementById('blVal').innerText=d.blIntensity;
    document.getElementById('blintensity').value=d.blIntensity;
    document.getElementById('blpattern').value=d.patternIdx||0;
    document.getElementById('nightH').value=d.nightStartH;
    document.getElementById('dayH').value=d.dayStartH;
    document.getElementById('tftDay').value=d.tftDay;document.getElementById('tftDayV').innerText=d.tftDay;
    document.getElementById('tftNight').value=d.tftNight;document.getElementById('tftNightV').innerText=d.tftNight;
    document.getElementById('blDay').value=d.blDay;document.getElementById('blDayV').innerText=d.blDay;
    document.getElementById('blNight').value=d.blNight;document.getElementById('blNightV').innerText=d.blNight;
    document.getElementById('tzOffset').value=d.tzOffset;
    var dm=document.getElementById('dimMode');
    dm.innerText=d.dimAuto?(d.isNight?'Night (Auto)':'Day (Auto)'):'Manual';
    dm.className='tag '+(d.dimAuto?'on':'off');
    document.getElementById('statusInfo').innerHTML='WiFi: '+d.ssid+' ('+d.rssi+' dBm) | IP: '+d.ip+' | Heap: '+d.freeHeap+' | Uptime: '+d.uptime+'s | FW: v'+d.firmware;
  }).catch(()=>{})
}
refresh();setInterval(refresh,2000);
</script>
</body></html>)rawliteral");

  return html;
}

String WebOTAManager::buildWifiPage()
{
  // Scan networks
  int n = WiFi.scanNetworks(false, false, false, 300);
  String nets = "";
  for (int i = 0; i < n && i < 15; i++)
  {
    nets += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)</option>";
  }
  WiFi.scanDelete();

  String html = F(R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WiFi Settings</title>
<style>
  body{font-family:-apple-system,sans-serif;max-width:400px;margin:0 auto;padding:20px;background:#1a1a2e;color:#eee}
  h1{color:#e94560;text-align:center}
  .card{background:#16213e;border-radius:8px;padding:15px;margin:10px 0}
  label{display:block;margin:10px 0 4px;font-size:0.9em;color:#ccc}
  input,select{width:100%;padding:10px;border:1px solid #333;border-radius:4px;background:#0f3460;color:#eee;box-sizing:border-box}
  button{width:100%;padding:12px;margin-top:16px;border:none;border-radius:4px;background:#e94560;color:white;font-weight:bold;font-size:1.1em;cursor:pointer}
  button:hover{background:#c73e54}
  nav{text-align:center;margin:10px 0}
  nav a{margin:0 10px;color:#e94560}
  .cur{color:#0f3;font-size:0.9em}
</style>
</head><body>
<h1>WiFi Settings</h1>
<nav><a href="/">Home</a> | <a href="/wifi">WiFi</a> | <a href="/update">OTA Update</a></nav>
<div class="card">
  <p class="cur">Currently connected to: <b>)rawliteral");

  html += WiFi.SSID();
  html += F("</b> (");
  html += WiFi.localIP().toString();
  html += F(R"rawliteral()</p>
  <form method="POST" action="/wifi/save">
    <label>Select Network:</label>
    <select onchange="document.getElementById('ssid').value=this.value">
      <option value="">-- Select --</option>)rawliteral");

  html += nets;
  html += F(R"rawliteral(
    </select>
    <label>SSID:</label>
    <input type="text" name="ssid" id="ssid" maxlength="31" required>
    <label>Password:</label>
    <input type="password" name="pass" maxlength="31">
    <button type="submit">Save & Reconnect</button>
  </form>
</div>
</body></html>)rawliteral");

  return html;
}

void WebOTAManager::handleWifiSave()
{
  if (!_server.hasArg("ssid") || _server.arg("ssid").length() == 0)
  {
    _server.send(400, "text/plain", "SSID is required");
    return;
  }

  String ssid = _server.arg("ssid");
  String pass = _server.hasArg("pass") ? _server.arg("pass") : "";

  snprintf(_config->config.wifi.ssid, sizeof(_config->config.wifi.ssid), "%s", ssid.c_str());
  snprintf(_config->config.wifi.password, sizeof(_config->config.wifi.password), "%s", pass.c_str());
  _config->config.wifi.WPS_connected = StoredConfig::valid;
  _config->save();

  Serial.printf("WebUI: WiFi credentials saved for SSID '%s'. Restarting...\n", ssid.c_str());

  _server.send(200, "text/html",
               "<html><body style='background:#1a1a2e;color:#eee;text-align:center;padding:40px;font-family:sans-serif'>"
               "<h1 style='color:#0f3'>WiFi Saved!</h1><p>Clock will restart and connect to the new network.</p></body></html>");

  delay(2000);
  ESP.restart();
}

String WebOTAManager::buildStatusJson()
{
  char buf[700];
  snprintf(buf, sizeof(buf),
           "{\"time\":\"%02d:%02d:%02d\",\"date\":\"%04d-%02d-%02d\","
           "\"brightness\":%d,\"graphic\":%d,\"graphicMax\":%d,"
           "\"blIntensity\":%d,\"blPower\":%s,\"displayPower\":%s,"
           "\"timeFormat\":\"%s\",\"patternIdx\":%d,\"pattern\":\"%s\","
           "\"nightStartH\":%d,\"dayStartH\":%d,"
           "\"tftDay\":%d,\"tftNight\":%d,\"blDay\":%d,\"blNight\":%d,"
           "\"dimAuto\":%s,\"isNight\":%s,"
           "\"tzOffset\":%.2f,"
           "\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,"
           "\"freeHeap\":%u,\"uptime\":%lu,\"firmware\":\"%s\"}",
           _uclock->getHour24(), _uclock->getMinute(), _uclock->getSecond(),
           _uclock->getYear(), _uclock->getMonth(), _uclock->getDay(),
           _tfts->dimming, _uclock->getActiveGraphicIdx(), _tfts->NumberOfClockFaces,
           _backlights->getIntensity(),
           _backlights->getPower() ? "true" : "false",
           _tfts->isEnabled() ? "true" : "false",
           _uclock->getTwelveHour() ? "12h" : "24h",
           _backlights->getPattern(), _backlights->getPatternStr().c_str(),
           _dimming->getNightStartHour(), _dimming->getDayStartHour(),
           _dimming->getTftDay(), _dimming->getTftNight(),
           _dimming->getBlDay(), _dimming->getBlNight(),
           _dimming->isAutoActive() ? "true" : "false",
           _dimming->isNight() ? "true" : "false",
           (double)_uclock->getTimeZoneOffset() / 3600.0,
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
      _dimming->setManualTftBrightness(v);
  }
  else if (cmd == "backlight")
  {
    int v = val.toInt();
    if (v >= 0 && v <= 7)
      _dimming->setManualBlIntensity(v);
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
  else if (cmd == "pattern")
  {
    int v = val.toInt();
    if (v >= 0 && v < Backlights::num_patterns)
      _backlights->setPattern(static_cast<Backlights::patterns>(v));
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
  else if (cmd == "timezone")
  {
    float tz = val.toFloat();
    if (tz >= -12.0f && tz <= 14.0f)
    {
      _uclock->setTimeZoneOffset((int32_t)(tz * 3600.0f));
      Serial.printf("WebUI: timezone offset set to %.2f hours\n", tz);
    }
  }
  else if (cmd == "dim_auto")
  {
    if (val == "on")
      _dimming->enableAuto();
    else if (val == "off")
    {
      // Just mark as manual without changing current values
      _dimming->setManualTftBrightness(_tfts->dimming);
    }
  }
  else if (cmd == "dim_night_h")
  {
    _dimming->setNightStartHour(val.toInt());
  }
  else if (cmd == "dim_day_h")
  {
    _dimming->setDayStartHour(val.toInt());
  }
  else if (cmd == "dim_tft_day")
  {
    _dimming->setTftDay(val.toInt());
  }
  else if (cmd == "dim_tft_night")
  {
    _dimming->setTftNight(val.toInt());
  }
  else if (cmd == "dim_bl_day")
  {
    _dimming->setBlDay(val.toInt());
  }
  else if (cmd == "dim_bl_night")
  {
    _dimming->setBlNight(val.toInt());
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
