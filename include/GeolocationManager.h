#ifndef GEOLOCATION_MANAGER_H
#define GEOLOCATION_MANAGER_H

#include "GLOBAL_DEFINES.h"

#ifdef GEOLOCATION_ENABLED

#include "Clock.h"
#include "StoredConfig.h"
#include "IPGeolocation_AO.h"

class GeolocationManager
{
public:
  void begin(Clock *uclock, StoredConfig *config);

  /// Initial query at startup. Returns true if timezone was updated.
  bool queryTimezoneOffset();

  /// Call every loop iteration to check if a periodic update is needed
  void checkUpdateNeeded();

  /// Call in free-time slots to perform the actual update if needed
  void processUpdate();

private:
  Clock *_uclock = nullptr;
  StoredConfig *_config = nullptr;

  double _tzOffset = 0;
  uint8_t _yesterday = 0;
  bool _needsUpdate = false;
  uint8_t _failedAttempts = 0;
  uint32_t _nextRetryMillis = 0;
  uint8_t _attemptDay = 0;

  static constexpr uint8_t MAX_FAILURES_PER_DAY = 4;
  static constexpr uint32_t RETRY_BACKOFF_MS = 5UL * 60UL * 1000UL;

  bool fetchTimezoneOffset();
};

#else // GEOLOCATION not enabled

class GeolocationManager
{
public:
  void begin(void *, void *) {}
  bool queryTimezoneOffset() { return false; }
  void checkUpdateNeeded() {}
  void processUpdate() {}
};

#endif // GEOLOCATION_ENABLED

#endif // GEOLOCATION_MANAGER_H
