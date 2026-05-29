#include "GeolocationManager.h"

#ifdef GEOLOCATION_ENABLED

#include <TimeLib.h>
#include <math.h>

void GeolocationManager::begin(Clock *uclock, StoredConfig *config)
{
  _uclock = uclock;
  _config = config;
}

bool GeolocationManager::queryTimezoneOffset()
{
  if (fetchTimezoneOffset())
  {
    _uclock->setTimeZoneOffset(_tzOffset * 3600);
    Serial.println();
    Serial.print("Saving config! Triggered by timezone change...");
    _config->save();
    Serial.println("Done!");
    return true;
  }
  return false;
}

void GeolocationManager::checkUpdateNeeded()
{
  uint8_t currentDay = _uclock->getDay();
  const uint8_t currentWeekday = weekday(_uclock->loop_time);
  const bool isSunday = (currentWeekday == 1);

  const bool isGeoLocWindow = isSunday && (currentDay != _yesterday) &&
                              (_uclock->getHour24() == 3) && (_uclock->getMinute() == 0) && (_uclock->getSecond() > 5);

  if (!_needsUpdate && isGeoLocWindow)
  {
    Serial.printf("GeoLoc needs update! Current date (DD.MM.YYYY): %d.%d.%d\n",
                  currentDay, _uclock->getMonth(), _uclock->getYear());

    _needsUpdate = true;
    _failedAttempts = 0;
    _nextRetryMillis = 0;
    _attemptDay = currentDay;
  }
}

void GeolocationManager::processUpdate()
{
  if (!_needsUpdate)
    return;

  const uint32_t now = millis();
  const uint8_t today = _uclock->getDay();

  if (_attemptDay != today)
  {
    _failedAttempts = 0;
    _nextRetryMillis = 0;
    _attemptDay = today;
  }

  if (_failedAttempts >= MAX_FAILURES_PER_DAY)
  {
    Serial.println("GeoLocation update skipped: failure limit reached for today.");
    _needsUpdate = false;
    return;
  }

  if (now < _nextRetryMillis)
    return;

  Serial.println("Daily update for geolocation timezone offset...");

  const int32_t oldOffset = _uclock->getTimeZoneOffset() / 3600;
  Serial.printf("Current TZ offset (hours): %d\n", oldOffset);
  Serial.println("Querying GeoLocation API...");

  if (fetchTimezoneOffset())
  {
    _uclock->setTimeZoneOffset(_tzOffset * 3600);
    const int32_t newOffset = _uclock->getTimeZoneOffset() / 3600;
    Serial.printf("New TZ offset (hours): %d\n", newOffset);

    _needsUpdate = false;
    _failedAttempts = 0;
    _nextRetryMillis = 0;
    _yesterday = today;
    return;
  }

  _failedAttempts++;
  if (_failedAttempts >= MAX_FAILURES_PER_DAY)
  {
    Serial.println("GeoLocation update aborted after repeated failures today.");
    _needsUpdate = false;
    return;
  }

  _nextRetryMillis = now + RETRY_BACKOFF_MS;
  Serial.printf("GeoLocation retry scheduled in %lu seconds.\n", RETRY_BACKOFF_MS / 1000);
}

bool GeolocationManager::fetchTimezoneOffset()
{
  Serial.println("\nStarting Geolocation API query...");

#ifdef GEOLOCATION_PROVIDER_IPAPI
  IPGeolocation location(GEOLOCATION_API_KEY, "IPAPI");
#elif defined(GEOLOCATION_PROVIDER_IPGEOLOCATION)
  IPGeolocation location(GEOLOCATION_API_KEY, "IPGEOLOCATION");
#elif defined(GEOLOCATION_PROVIDER_ABSTRACTAPI)
  IPGeolocation location(GEOLOCATION_API_KEY, "ABSTRACTAPI");
#else
  IPGeolocation location(GEOLOCATION_API_KEY, "IPAPI");
#endif

  IPGeo ipg;
  if (location.updateStatus(&ipg))
  {
    Serial.println(String("Geo Time Zone: ") + String(ipg.tz));
    Serial.println(String("Geo TZ Offset: ") + String(ipg.offset));
    Serial.println(String("Geo Current Time: ") + String(ipg.current_time));

    const double rawOffsetHours = ipg.offset;
    const int32_t newOffsetSeconds = static_cast<int32_t>(lround(rawOffsetHours * 3600.0));

    if ((newOffsetSeconds % (15 * 60)) != 0)
    {
      Serial.printf("GeoLoc rejected offset not aligned to 15 min grid (seconds): %d\n", newOffsetSeconds);
      return false;
    }

    const bool hasValidStoredOffset = _config->config.uclock.is_valid == StoredConfig::valid;
    const int32_t previousOffsetSeconds = static_cast<int32_t>(_config->config.uclock.time_zone_offset);
    const int32_t defaultOffsetSeconds = 1 * 3600;

    if (hasValidStoredOffset && previousOffsetSeconds != 0 && previousOffsetSeconds != defaultOffsetSeconds)
    {
      int32_t diff = newOffsetSeconds - previousOffsetSeconds;
      if (diff < 0)
        diff = -diff;

      if (diff > (2 * 3600))
      {
        Serial.printf("GeoLoc offset deviates by more than 2h from stored value (prev: %ds, new: %ds). Ignoring update.\n",
                      previousOffsetSeconds, newOffsetSeconds);
        return false;
      }
    }

    _tzOffset = static_cast<double>(newOffsetSeconds) / 3600.0;
    Serial.println(String("Geo TZ Offset (applied): ") + String(_tzOffset));
    return true;
  }

  Serial.println("Geolocation failed.");
  return false;
}

#endif // GEOLOCATION_ENABLED
