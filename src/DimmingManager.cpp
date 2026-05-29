#include "DimmingManager.h"

#ifdef DIMMING

void DimmingManager::begin(Clock *uclock, TFTs *tfts, Backlights *backlights, StoredConfig *config)
{
  _uclock = uclock;
  _tfts = tfts;
  _backlights = backlights;
  _config = config;

  // Initialize dimming config with defaults if not yet valid
  if (_config->config.dimming.is_valid != StoredConfig::valid)
  {
    Serial.println("Dimming config not valid, writing defaults.");
    _config->config.dimming.tft_brightness_day = 255;
    _config->config.dimming.tft_brightness_night = TFT_DIMMED_INTENSITY;
    _config->config.dimming.bl_intensity_day = 7;
    _config->config.dimming.bl_intensity_night = BACKLIGHT_DIMMED_INTENSITY;
    _config->config.dimming.auto_enabled = true;
    _config->config.dimming.night_start_hour = NIGHT_TIME;
    _config->config.dimming.day_start_hour = DAY_TIME;
    _config->config.dimming.is_valid = StoredConfig::valid;
  }

  _autoActive = _config->config.dimming.auto_enabled;
  _initialized = false;
}

bool DimmingManager::check()
{
  uint8_t current_hour = _uclock->getHour24();
  bool night = isNightTime(current_hour);

  // First call: apply immediately without needing a transition
  if (!_initialized)
  {
    _initialized = true;
    _wasNight = night;
    if (_autoActive)
    {
      Serial.printf("Dimming init: %s mode (hour=%d, night=%d-%d)\n",
                    night ? "NIGHT" : "DAY", current_hour,
                    _config->config.dimming.night_start_hour,
                    _config->config.dimming.day_start_hour);
      applyDimming(night);
      return true;
    }
    return false;
  }

  // Only act on day↔night transitions, not every hour
  if (night == _wasNight)
    return false;

  _wasNight = night;

  // On transition, re-enable auto (override only holds until next transition)
  if (!_autoActive)
  {
    Serial.println("Dimming: day/night transition, re-enabling auto mode.");
    _autoActive = true;
    _config->config.dimming.auto_enabled = true;
  }

  Serial.printf("Dimming transition to %s mode (hour=%d)\n",
                night ? "NIGHT" : "DAY", current_hour);
  applyDimming(night);
  return true;
}

void DimmingManager::applyDimming(bool night)
{
  if (night)
  {
    _tfts->dimming = _config->config.dimming.tft_brightness_night;
    _tfts->ProcessUpdatedDimming();
    _backlights->setIntensity(_config->config.dimming.bl_intensity_night);
    _backlights->setDimming(true);
  }
  else
  {
    _tfts->dimming = _config->config.dimming.tft_brightness_day;
    _tfts->ProcessUpdatedDimming();
    _backlights->setIntensity(_config->config.dimming.bl_intensity_day);
    _backlights->setDimming(false);
  }
}

void DimmingManager::setManualTftBrightness(uint8_t value)
{
  _tfts->dimming = value;
  _tfts->ProcessUpdatedDimming();
  _autoActive = false;
  _config->config.dimming.auto_enabled = false;
  Serial.printf("Dimming: manual TFT brightness set to %d, auto disabled until next transition.\n", value);
}

void DimmingManager::setManualBlIntensity(uint8_t value)
{
  _backlights->setIntensity(value);
  _autoActive = false;
  _config->config.dimming.auto_enabled = false;
  Serial.printf("Dimming: manual backlight intensity set to %d, auto disabled until next transition.\n", value);
}

void DimmingManager::enableAuto()
{
  _autoActive = true;
  _config->config.dimming.auto_enabled = true;
  // Apply current state immediately
  bool night = isNightTime(_uclock->getHour24());
  applyDimming(night);
  Serial.println("Dimming: auto mode re-enabled.");
}

bool DimmingManager::isNight() const
{
  return isNightTime(_uclock->getHour24());
}

bool DimmingManager::isNightTime(uint8_t current_hour) const
{
  uint8_t nightStart = _config->config.dimming.night_start_hour;
  uint8_t dayStart = _config->config.dimming.day_start_hour;

  if (dayStart < nightStart)
  {
    // Night spans midnight: e.g. 22:00 - 07:00
    return (current_hour >= nightStart) || (current_hour < dayStart);
  }
  else
  {
    // Night within same day: e.g. 02:00 - 10:00 (unusual but supported)
    return (current_hour >= nightStart) && (current_hour < dayStart);
  }
}

// --- Getters ---
uint8_t DimmingManager::getTftDay() const { return _config->config.dimming.tft_brightness_day; }
uint8_t DimmingManager::getTftNight() const { return _config->config.dimming.tft_brightness_night; }
uint8_t DimmingManager::getBlDay() const { return _config->config.dimming.bl_intensity_day; }
uint8_t DimmingManager::getBlNight() const { return _config->config.dimming.bl_intensity_night; }
uint8_t DimmingManager::getNightStartHour() const { return _config->config.dimming.night_start_hour; }
uint8_t DimmingManager::getDayStartHour() const { return _config->config.dimming.day_start_hour; }

// --- Setters ---
void DimmingManager::setTftDay(uint8_t v) { _config->config.dimming.tft_brightness_day = v; }
void DimmingManager::setTftNight(uint8_t v) { _config->config.dimming.tft_brightness_night = v; }
void DimmingManager::setBlDay(uint8_t v) { _config->config.dimming.bl_intensity_day = v; }
void DimmingManager::setBlNight(uint8_t v) { _config->config.dimming.bl_intensity_night = v; }
void DimmingManager::setNightStartHour(uint8_t h) { _config->config.dimming.night_start_hour = (h < 24) ? h : 22; }
void DimmingManager::setDayStartHour(uint8_t h) { _config->config.dimming.day_start_hour = (h < 24) ? h : 7; }

#endif // DIMMING
