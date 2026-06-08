#ifndef DIMMING_MANAGER_H
#define DIMMING_MANAGER_H

#include "GLOBAL_DEFINES.h"

#ifdef DIMMING

#include "Clock.h"
#include "TFTs.h"
#include "Backlights.h"
#include "StoredConfig.h"

class DimmingManager
{
public:
  void begin(Clock *uclock, TFTs *tfts, Backlights *backlights, StoredConfig *config);
  bool check();

private:
  Clock *_uclock = nullptr;
  TFTs *_tfts = nullptr;
  Backlights *_backlights = nullptr;
  StoredConfig *_config = nullptr;

  bool _wasNight = false;
  bool _initialized = false;

  bool isNightTime(uint8_t current_hour) const;
  bool applyNight();
  bool applyDay();
};

#else

class DimmingManager
{
public:
  void begin(void *, void *, void *, void *) {}
  bool check() { return false; }
};

#endif

#endif
