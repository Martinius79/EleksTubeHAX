#include "DimmingManager.h"

#ifdef DIMMING

void DimmingManager::begin(Clock *uclock, TFTs *tfts, Backlights *backlights)
{
  _uclock = uclock;
  _tfts = tfts;
  _backlights = backlights;
}

bool DimmingManager::check()
{
  uint8_t current_hour = _uclock->getHour24();

  if (current_hour == _hourOld)
    return false;

  Serial.printf("Current hour = %d, Night Time Start = %d, Day Time Start = %d\n",
                current_hour, NIGHT_TIME, DAY_TIME);

  if (isNightTime(current_hour))
  {
    Serial.println("Set to night time mode (dimmed)!");
    _tfts->dimming = TFT_DIMMED_INTENSITY;
    _tfts->ProcessUpdatedDimming();
    _backlights->setDimming(true);
  }
  else
  {
    Serial.println("Set to day time mode (max brightness)!");
    _tfts->dimming = 255;
    _tfts->ProcessUpdatedDimming();
    // _backlights->setDimming(false);
  }

  _hourOld = current_hour;
  return true; // Signal that a display refresh is needed
}

bool DimmingManager::isNightTime(uint8_t current_hour)
{
  if (DAY_TIME < NIGHT_TIME)
  {
    // "Night" spans across midnight
    return (current_hour < DAY_TIME) || (current_hour >= NIGHT_TIME);
  }
  else
  {
    // "Night" starts after midnight, entirely within current day
    return (current_hour >= NIGHT_TIME) && (current_hour < DAY_TIME);
  }
}

#endif // DIMMING
