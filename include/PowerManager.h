#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "GLOBAL_DEFINES.h"
#include "TFTs.h"
#include "Backlights.h"

class Clock;

class PowerManager
{
public:
  void begin(TFTs *tfts, Backlights *backlights, Clock *uclock);

  void handlePowerButton();
  void handlePowerToggle();

private:
  TFTs *_tfts = nullptr;
  Backlights *_backlights = nullptr;
  Clock *_uclock = nullptr;

  void powerOff();
  void powerOn(bool useReinit);
};

#endif
