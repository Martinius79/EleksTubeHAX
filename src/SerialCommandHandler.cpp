#include "SerialCommandHandler.h"
#include <WiFi.h>

void SerialCommandHandler::begin(Clock *uclock, TFTs *tfts, Backlights *backlights, StoredConfig *config)
{
  _uclock = uclock;
  _tfts = tfts;
  _backlights = backlights;
  _config = config;
  _inputBuffer.reserve(64);
}

void SerialCommandHandler::handle()
{
  while (Serial.available())
  {
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      if (_inputBuffer.length() > 0)
      {
        processCommand(_inputBuffer);
        _inputBuffer = "";
      }
    }
    else
    {
      if (_inputBuffer.length() < 64)
        _inputBuffer += c;
    }
  }
}

void SerialCommandHandler::processCommand(const String &cmd)
{
  String trimmed = cmd;
  trimmed.trim();
  String lower = trimmed;
  lower.toLowerCase();

  if (lower == "help" || lower == "?")
    printHelp();
  else if (lower == "status")
    printStatus();
  else if (lower == "time")
    printTime();
  else if (lower == "network" || lower == "wifi")
    printNetwork();
  else if (lower.startsWith("brightness "))
    setBrightness(trimmed.substring(11));
  else if (lower.startsWith("backlight "))
    setBacklightBrightness(trimmed.substring(10));
  else if (lower.startsWith("pattern "))
    setPattern(trimmed.substring(8));
  else if (lower.startsWith("graphic "))
    setGraphic(trimmed.substring(8));
  else if (lower.startsWith("timeformat "))
    setTimeFormat(trimmed.substring(11));
  else if (lower.startsWith("power "))
    setPower(trimmed.substring(6));
  else if (lower.startsWith("blpower "))
    setBacklightPower(trimmed.substring(8));
  else if (lower == "save")
    saveConfig();
  else if (lower == "reboot" || lower == "restart")
    reboot();
  else
    Serial.println("Unknown command. Type 'help' for available commands.");
}

void SerialCommandHandler::printHelp()
{
  Serial.println(F("=== Serial Command Interface ==="));
  Serial.println(F("Commands:"));
  Serial.println(F("  help / ?          - Show this help"));
  Serial.println(F("  status            - Show current status"));
  Serial.println(F("  time              - Show current time"));
  Serial.println(F("  network / wifi    - Show network info"));
  Serial.println(F("  brightness <0-255> - Set display brightness"));
  Serial.println(F("  backlight <0-7>   - Set backlight intensity"));
  Serial.println(F("  pattern <name>    - Set backlight pattern"));
  Serial.println(F("  graphic <1-N>     - Set clock face graphic"));
  Serial.println(F("  timeformat <12|24> - Set 12/24 hour format"));
  Serial.println(F("  power <on|off>    - Turn displays on/off"));
  Serial.println(F("  blpower <on|off>  - Turn backlight on/off"));
  Serial.println(F("  save              - Save current config"));
  Serial.println(F("  reboot / restart  - Restart the device"));
}

void SerialCommandHandler::printStatus()
{
  Serial.println(F("=== Status ==="));
  Serial.printf("Firmware: v%s\n", FIRMWARE_VERSION);
  Serial.printf("Display: %s\n", _tfts->isEnabled() ? "ON" : "OFF");
  Serial.printf("Display Brightness: %d\n", _tfts->dimming);
  Serial.printf("Backlight: %s\n", _backlights->getPower() ? "ON" : "OFF");
  Serial.printf("Backlight Intensity: %d\n", _backlights->getIntensity());
  Serial.printf("Backlight Pattern: %s\n", _backlights->getPatternStr().c_str());
  Serial.printf("Clock Face: %d / %d\n", _uclock->getActiveGraphicIdx(), _tfts->NumberOfClockFaces);
  Serial.printf("Time Format: %s\n", _uclock->getTwelveHour() ? "12h" : "24h");
  Serial.printf("UTC Offset: %ld sec\n", (long)_uclock->getTimeZoneOffset());
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Uptime: %lu sec\n", millis() / 1000);
}

void SerialCommandHandler::printTime()
{
  Serial.printf("Time: %02d:%02d:%02d (%s)\n",
                _uclock->getHour24(), _uclock->getMinute(), _uclock->getSecond(),
                _uclock->getTwelveHour() ? "12h mode" : "24h mode");
  Serial.printf("Date: %04d-%02d-%02d\n",
                _uclock->getYear(), _uclock->getMonth(), _uclock->getDay());
}

void SerialCommandHandler::printNetwork()
{
  Serial.println(F("=== Network ==="));
  Serial.printf("WiFi Status: %s\n", WiFi.isConnected() ? "Connected" : "Disconnected");
  if (WiFi.isConnected())
  {
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
  }
}

void SerialCommandHandler::setBrightness(const String &args)
{
  int val = args.toInt();
  if (val < 0 || val > 255)
  {
    Serial.println("Error: brightness must be 0-255");
    return;
  }
  _tfts->dimming = val;
  _tfts->ProcessUpdatedDimming();
  Serial.printf("Display brightness set to %d\n", val);
}

void SerialCommandHandler::setBacklightBrightness(const String &args)
{
  int val = args.toInt();
  if (val < 0 || val > 7)
  {
    Serial.println("Error: backlight intensity must be 0-7");
    return;
  }
  _backlights->setIntensity(val);
  Serial.printf("Backlight intensity set to %d\n", val);
}

void SerialCommandHandler::setPattern(const String &args)
{
  String pattern = args;
  pattern.trim();
  for (int8_t i = 0; i < Backlights::num_patterns; i++)
  {
    if (pattern.equalsIgnoreCase(Backlights::patterns_str[i]))
    {
      _backlights->setPattern(Backlights::patterns(i));
      Serial.printf("Pattern set to: %s\n", Backlights::patterns_str[i].c_str());
      return;
    }
  }
  Serial.print("Unknown pattern. Available: ");
  for (int8_t i = 0; i < Backlights::num_patterns; i++)
  {
    Serial.print(Backlights::patterns_str[i]);
    if (i < Backlights::num_patterns - 1)
      Serial.print(", ");
  }
  Serial.println();
}

void SerialCommandHandler::setGraphic(const String &args)
{
  int val = args.toInt();
  if (val < 1 || val > _tfts->NumberOfClockFaces)
  {
    Serial.printf("Error: graphic must be 1-%d\n", _tfts->NumberOfClockFaces);
    return;
  }
  _uclock->setClockGraphicsIdx(val);
  _tfts->current_graphic = _uclock->getActiveGraphicIdx();
  Serial.printf("Clock face set to %d\n", val);
}

void SerialCommandHandler::setTimeFormat(const String &args)
{
  String fmt = args;
  fmt.trim();
  if (fmt == "12")
  {
    _uclock->setTwelveHour(true);
    Serial.println("Time format set to 12h");
  }
  else if (fmt == "24")
  {
    _uclock->setTwelveHour(false);
    Serial.println("Time format set to 24h");
  }
  else
  {
    Serial.println("Error: use 'timeformat 12' or 'timeformat 24'");
  }
}

void SerialCommandHandler::setPower(const String &args)
{
  String val = args;
  val.trim();
  val.toLowerCase();
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
    Serial.println("Displays ON");
  }
  else if (val == "off")
  {
    _tfts->chip_select.setAll();
    _tfts->fillScreen(TFT_BLACK);
    _tfts->disableAllDisplays();
    Serial.println("Displays OFF");
  }
  else
  {
    Serial.println("Error: use 'power on' or 'power off'");
  }
}

void SerialCommandHandler::setBacklightPower(const String &args)
{
  String val = args;
  val.trim();
  val.toLowerCase();
  if (val == "on")
  {
    _backlights->PowerOn();
    Serial.println("Backlight ON");
  }
  else if (val == "off")
  {
    _backlights->PowerOff();
    Serial.println("Backlight OFF");
  }
  else
  {
    Serial.println("Error: use 'blpower on' or 'blpower off'");
  }
}

void SerialCommandHandler::saveConfig()
{
  Serial.print("Saving config...");
  _config->save();
  Serial.println(" Done.");
}

void SerialCommandHandler::reboot()
{
  Serial.println("Rebooting...");
  delay(100);
  ESP.restart();
}
