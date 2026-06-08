#include "DimmingManager.h"
#include "DisplayHelpers.h"

#ifdef DIMMING

void DimmingManager::begin(Clock *uclock, TFTs *tfts, Backlights *backlights, StoredConfig *config)
{
  _uclock = uclock;
  _tfts = tfts;
  _backlights = backlights;
  _config = config;
}

bool DimmingManager::check()
{
  uint8_t current_hour = _uclock->getHour24();
  bool night = isNightTime(current_hour);

  if (!_initialized)
  {
    _initialized = true;
    _wasNight = night;
    bool changed = night ? applyNight() : applyDay();
    return changed;
  }

  if (night == _wasNight)
    return false;

  _wasNight = night;
  Serial.printf("Current hour = %d, Night Time Start = %d, Day Time Start = %d\n",
                current_hour,
                (uint8_t)NIGHT_TIME,
                (uint8_t)DAY_TIME);

  bool changed;
  if (night)
  {
    Serial.println("Set to night time mode (dimmed)!");
    changed = applyNight();
  }
  else
  {
    Serial.println("Set to day time mode (max brightness)!");
    changed = applyDay();
  }

  if (changed)
  {
    updateClockDisplay(*_tfts, *_uclock, TFTs::force);
  }

  return changed;
}

bool DimmingManager::isNightTime(uint8_t current_hour) const
{
  if (DAY_TIME < NIGHT_TIME)
  {
    return (current_hour < DAY_TIME) || (current_hour >= NIGHT_TIME);
  }
  else
  {
    return (current_hour >= NIGHT_TIME) && (current_hour < DAY_TIME);
  }
}

bool DimmingManager::applyNight()
{
  _tfts->dimming = TFT_DIMMED_INTENSITY;
  _tfts->ProcessUpdatedDimming();
  _backlights->setDimming(true);
  return true;
}

bool DimmingManager::applyDay()
{
  _tfts->dimming = 255;
  _tfts->ProcessUpdatedDimming();
  return true;
}

#endif
