#ifndef DIMMING_MANAGER_H
#define DIMMING_MANAGER_H

#include "GLOBAL_DEFINES.h"

#ifdef DIMMING

#include "Clock.h"
#include "TFTs.h"
#include "Backlights.h"

class DimmingManager
{
public:
  void begin(Clock *uclock, TFTs *tfts, Backlights *backlights);

  /// Check if dimming state needs to change. Returns true if display was updated.
  bool check();

private:
  Clock *_uclock = nullptr;
  TFTs *_tfts = nullptr;
  Backlights *_backlights = nullptr;
  uint8_t _hourOld = 255;

  bool isNightTime(uint8_t current_hour);
};

#else // DIMMING not enabled

class DimmingManager
{
public:
  void begin(void *, void *, void *) {}
  bool check() { return false; }
};

#endif // DIMMING

#endif // DIMMING_MANAGER_H
