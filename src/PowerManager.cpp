#include "PowerManager.h"
#include "DisplayHelpers.h"
#include "Clock.h"

void PowerManager::begin(TFTs *tfts, Backlights *backlights, Clock *uclock)
{
  _tfts = tfts;
  _backlights = backlights;
  _uclock = uclock;
}

void PowerManager::handlePowerButton()
{
  if (_tfts->isEnabled())
  {
    powerOff();
  }
  else
  {
#ifdef HARDWARE_ELEKSTUBE_CLOCK
    powerOn(true);
#else
    powerOn(false);
#endif
  }
}

void PowerManager::handlePowerToggle()
{
  if (_tfts->isEnabled())
  {
    powerOff();
  }
  else
  {
    powerOn(false);
  }
}

void PowerManager::powerOff()
{
  _tfts->chip_select.setAll();
  _tfts->fillScreen(TFT_BLACK);
  _tfts->disableAllDisplays();
  _backlights->PowerOff();
}

void PowerManager::powerOn(bool useReinit)
{
  if (useReinit)
  {
    _tfts->reinit();
  }
  else
  {
    _tfts->enableAllDisplays();
  }
  updateClockDisplay(*_tfts, *_uclock, TFTs::force);
  _backlights->PowerOn();
}
