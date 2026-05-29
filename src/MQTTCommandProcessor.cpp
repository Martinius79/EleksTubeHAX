#include "MQTTCommandProcessor.h"

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)

#define MQTT_SAVE_PREFERENCES_AFTER_SEC 60

void MQTTCommandProcessor::begin(TFTs *tfts, Backlights *backlights, Clock *uclock, StoredConfig *config, Menu *menu)
{
  _tfts = tfts;
  _backlights = backlights;
  _uclock = uclock;
  _config = config;
  _menu = menu;
}

void MQTTCommandProcessor::loop()
{
  MQTTLoopFrequently();
  processCommands();
  updateStatus();

  // Save config after a delay if an MQTT command was received and we are not in the menu
  if (_lastCommandExecuted != (uint32_t)-1)
  {
    if (((millis() - _lastCommandExecuted) > (MQTT_SAVE_PREFERENCES_AFTER_SEC * 1000)) && _menu->getState() == Menu::idle)
    {
      _lastCommandExecuted = (uint32_t)-1;
      Serial.print("Saving config...");
      _config->save();
      Serial.println(" Done.");
    }
  }
}

void MQTTCommandProcessor::loopInFreeTime()
{
  MQTTLoopInFreeTime();
}

void MQTTCommandProcessor::processCommands()
{
  bool commandReceived =
      MQTTCommandMainPowerReceived ||
      MQTTCommandBackPowerReceived ||
      MQTTCommandStateReceived ||
      MQTTCommandBrightnessReceived ||
      MQTTCommandMainBrightnessReceived ||
      MQTTCommandBackBrightnessReceived ||
      MQTTCommandPatternReceived ||
      MQTTCommandBackPatternReceived ||
      MQTTCommandBackColorPhaseReceived ||
      MQTTCommandGraphicReceived ||
      MQTTCommandMainGraphicReceived ||
      MQTTCommandUseTwelveHoursReceived ||
      MQTTCommandBlankZeroHoursReceived ||
      MQTTCommandPulseBpmReceived ||
      MQTTCommandBreathBpmReceived ||
      MQTTCommandRainbowSecReceived;

  if (MQTTCommandMainPowerReceived)
    handleMainPower();
  if (MQTTCommandBackPowerReceived)
    handleBackPower();
  if (MQTTCommandStateReceived)
    handleState();
  if (MQTTCommandMainBrightnessReceived)
    handleMainBrightness();
  if (MQTTCommandBackBrightnessReceived)
    handleBackBrightness();
  if (MQTTCommandPatternReceived)
    handlePattern();
  if (MQTTCommandBackPatternReceived)
    handleBackPattern();
  if (MQTTCommandBackColorPhaseReceived)
    handleBackColorPhase();
  if (MQTTCommandGraphicReceived)
    handleGraphic();
  if (MQTTCommandMainGraphicReceived)
    handleMainGraphic();
  if (MQTTCommandUseTwelveHoursReceived)
    handleUseTwelveHours();
  if (MQTTCommandBlankZeroHoursReceived)
    handleBlankZeroHours();
  if (MQTTCommandPulseBpmReceived)
    handlePulseBpm();
  if (MQTTCommandBreathBpmReceived)
    handleBreathBpm();
  if (MQTTCommandRainbowSecReceived)
    handleRainbowSec();

  if (commandReceived)
  {
    _lastCommandExecuted = millis();
    MQTTReportBackEverything(true);
  }
}

void MQTTCommandProcessor::updateStatus()
{
  MQTTStatusMainPower = _tfts->isEnabled();
  MQTTStatusBackPower = _backlights->getPower();
  MQTTStatusState = (_uclock->getActiveGraphicIdx() + 1) * 5;
  MQTTStatusBrightness = _backlights->getIntensity();
  MQTTStatusMainBrightness = _tfts->dimming;
  MQTTStatusBackBrightness = _backlights->getIntensity();
  strcpy(MQTTStatusPattern, _backlights->getPatternStr().c_str());
  strcpy(MQTTStatusBackPattern, _backlights->getPatternStr().c_str());
  _backlights->getPatternStr().toCharArray(MQTTStatusBackPattern, _backlights->getPatternStr().length() + 1);
  MQTTStatusBackColorPhase = _backlights->getColorPhase();
  MQTTStatusGraphic = _uclock->getActiveGraphicIdx();
  MQTTStatusMainGraphic = _uclock->getActiveGraphicIdx();
  MQTTStatusUseTwelveHours = _uclock->getTwelveHour();
  MQTTStatusBlankZeroHours = _uclock->getBlankHoursZero();
  MQTTStatusPulseBpm = _backlights->getPulseRate();
  MQTTStatusBreathBpm = _backlights->getBreathRate();
  MQTTStatusRainbowSec = _backlights->getRainbowDuration();
}

void MQTTCommandProcessor::handleMainPower()
{
  MQTTCommandMainPowerReceived = false;
  if (MQTTCommandMainPower)
  {
    if (!_tfts->isEnabled())
    {
#ifdef HARDWARE_ELEKSTUBE_CLOCK
      _tfts->reinit();
#else
      _tfts->enableAllDisplays();
#endif
      updateClockDisplay(TFTs::force);
    }
  }
  else
  {
    _tfts->chip_select.setAll();
    _tfts->fillScreen(TFT_BLACK);
    _tfts->disableAllDisplays();
  }
}

void MQTTCommandProcessor::handleBackPower()
{
  MQTTCommandBackPowerReceived = false;
  if (MQTTCommandBackPower)
    _backlights->PowerOn();
  else
    _backlights->PowerOff();
}

void MQTTCommandProcessor::handleState()
{
  MQTTCommandStateReceived = false;
  randomSeed(millis());
  uint8_t idx;
  if (MQTTCommandState >= 90)
    idx = random(1, _tfts->NumberOfClockFaces + 1);
  else
    idx = (MQTTCommandState / 5) - 1;

  Serial.printf("Graphic change request from MQTT; command: %d, index: %d\n", MQTTCommandState, idx);
  _uclock->setClockGraphicsIdx(idx);
  _tfts->current_graphic = _uclock->getActiveGraphicIdx();
  updateClockDisplay(TFTs::force);
}

void MQTTCommandProcessor::handleMainBrightness()
{
  MQTTCommandMainBrightnessReceived = false;
  _tfts->dimming = MQTTCommandMainBrightness;
  _tfts->ProcessUpdatedDimming();
  updateClockDisplay(TFTs::force);
}

void MQTTCommandProcessor::handleBackBrightness()
{
  MQTTCommandBackBrightnessReceived = false;
  _backlights->setIntensity(uint8_t(MQTTCommandBackBrightness));
}

void MQTTCommandProcessor::handlePattern()
{
  MQTTCommandPatternReceived = false;
  for (int8_t i = 0; i < Backlights::num_patterns; i++)
  {
    if (strcmp(MQTTCommandPattern, (Backlights::patterns_str[i]).c_str()) == 0)
    {
      _backlights->setPattern(Backlights::patterns(i));
      break;
    }
  }
}

void MQTTCommandProcessor::handleBackPattern()
{
  MQTTCommandBackPatternReceived = false;
  for (int8_t i = 0; i < Backlights::num_patterns; i++)
  {
    if (strcmp(MQTTCommandBackPattern, (Backlights::patterns_str[i]).c_str()) == 0)
    {
      _backlights->setPattern(Backlights::patterns(i));
      break;
    }
  }
}

void MQTTCommandProcessor::handleBackColorPhase()
{
  MQTTCommandBackColorPhaseReceived = false;
  _backlights->setColorPhase(MQTTCommandBackColorPhase);
}

void MQTTCommandProcessor::handleGraphic()
{
  MQTTCommandGraphicReceived = false;
  _uclock->setClockGraphicsIdx(MQTTCommandGraphic);
  _tfts->current_graphic = _uclock->getActiveGraphicIdx();
  updateClockDisplay(TFTs::force);
}

void MQTTCommandProcessor::handleMainGraphic()
{
  MQTTCommandMainGraphicReceived = false;
  _uclock->setClockGraphicsIdx(MQTTCommandMainGraphic);
  _tfts->current_graphic = _uclock->getActiveGraphicIdx();
  updateClockDisplay(TFTs::force);
}

void MQTTCommandProcessor::handleUseTwelveHours()
{
  MQTTCommandUseTwelveHoursReceived = false;
  _uclock->setTwelveHour(MQTTCommandUseTwelveHours);
}

void MQTTCommandProcessor::handleBlankZeroHours()
{
  MQTTCommandBlankZeroHoursReceived = false;
  _uclock->setBlankHoursZero(MQTTCommandBlankZeroHours);
}

void MQTTCommandProcessor::handlePulseBpm()
{
  MQTTCommandPulseBpmReceived = false;
  _backlights->setPulseRate(MQTTCommandPulseBpm);
}

void MQTTCommandProcessor::handleBreathBpm()
{
  MQTTCommandBreathBpmReceived = false;
  _backlights->setBreathRate(MQTTCommandBreathBpm);
}

void MQTTCommandProcessor::handleRainbowSec()
{
  MQTTCommandRainbowSecReceived = false;
  _backlights->setRainbowDuration(MQTTCommandRainbowSec);
}

void MQTTCommandProcessor::updateClockDisplay(TFTs::show_t show)
{
  _tfts->setDigit(SECONDS_ONES, _uclock->getSecondsOnes(), show);
  _tfts->setDigit(SECONDS_TENS, _uclock->getSecondsTens(), show);
  _tfts->setDigit(MINUTES_ONES, _uclock->getMinutesOnes(), show);
  _tfts->setDigit(MINUTES_TENS, _uclock->getMinutesTens(), show);
  _tfts->setDigit(HOURS_ONES, _uclock->getHoursOnes(), show);
  _tfts->setDigit(HOURS_TENS, _uclock->getHoursTens(), show);
}

#endif // MQTT
