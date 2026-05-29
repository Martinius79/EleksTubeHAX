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

  /// Check if dimming state needs to change (day/night transition). Returns true if display was updated.
  bool check();

  /// Called when user manually sets TFT brightness - disables auto until next transition
  void setManualTftBrightness(uint8_t value);

  /// Called when user manually sets backlight intensity - disables auto until next transition
  void setManualBlIntensity(uint8_t value);

  /// Re-enable automatic dimming (e.g. from WebUI "Auto" button)
  void enableAuto();

  /// Check if currently in night mode
  bool isNight() const;

  /// Check if auto dimming is active (not overridden)
  bool isAutoActive() const { return _autoActive; }

  /// Get configured values
  uint8_t getTftDay() const;
  uint8_t getTftNight() const;
  uint8_t getBlDay() const;
  uint8_t getBlNight() const;
  uint8_t getNightStartHour() const;
  uint8_t getDayStartHour() const;

  /// Set configured values (persisted via StoredConfig)
  void setTftDay(uint8_t v);
  void setTftNight(uint8_t v);
  void setBlDay(uint8_t v);
  void setBlNight(uint8_t v);
  void setNightStartHour(uint8_t h);
  void setDayStartHour(uint8_t h);

private:
  Clock *_uclock = nullptr;
  TFTs *_tfts = nullptr;
  Backlights *_backlights = nullptr;
  StoredConfig *_config = nullptr;

  bool _autoActive = true;   // true = auto day/night switching active
  bool _wasNight = false;    // track last known state for transition detection
  bool _initialized = false; // first check applies immediately

  bool isNightTime(uint8_t current_hour) const;
  void applyDimming(bool night);
};

#else // DIMMING not enabled

class DimmingManager
{
public:
  void begin(void *, void *, void *, void *) {}
  bool check() { return false; }
  void setManualTftBrightness(uint8_t) {}
  void setManualBlIntensity(uint8_t) {}
  void enableAuto() {}
  bool isNight() const { return false; }
  bool isAutoActive() const { return true; }
  uint8_t getTftDay() const { return 255; }
  uint8_t getTftNight() const { return 20; }
  uint8_t getBlDay() const { return 7; }
  uint8_t getBlNight() const { return 1; }
  uint8_t getNightStartHour() const { return 22; }
  uint8_t getDayStartHour() const { return 7; }
  void setTftDay(uint8_t) {}
  void setTftNight(uint8_t) {}
  void setBlDay(uint8_t) {}
  void setBlNight(uint8_t) {}
  void setNightStartHour(uint8_t) {}
  void setDayStartHour(uint8_t) {}
};

#endif // DIMMING

#endif // DIMMING_MANAGER_H
