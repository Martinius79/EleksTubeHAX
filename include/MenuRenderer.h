#ifndef MENU_RENDERER_H
#define MENU_RENDERER_H

#include "GLOBAL_DEFINES.h"
#include "Menu.h"
#include "TFTs.h"
#include "Clock.h"
#include "Backlights.h"
#include "StoredConfig.h"

class MenuRenderer
{
public:
  void begin(TFTs *tfts, Clock *uclock, Backlights *backlights, StoredConfig *config);

  /// Call every loop iteration. Returns true if menu caused a display update.
  void loop(Menu &menu);

private:
  TFTs *_tfts = nullptr;
  Clock *_uclock = nullptr;
  Backlights *_backlights = nullptr;
  StoredConfig *_config = nullptr;

  void setupMenuDisplay();
  void updateClockDisplay(TFTs::show_t show = TFTs::yes);

  void renderBacklightPattern(int8_t change);
  void renderPatternColor(int8_t change);
  void renderBacklightIntensity(int8_t change);
  void renderTwelveHour(int8_t change);
  void renderBlankHoursZero(int8_t change);
  void renderUtcOffsetHour(int8_t change);
  void renderUtcOffset15m(int8_t change);
  void renderSelectedGraphic(int8_t change);
#ifdef WIFI_USE_WPS
  void renderStartWps(int8_t change);
#endif
};

#endif // MENU_RENDERER_H
