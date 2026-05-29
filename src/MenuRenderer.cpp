#include "MenuRenderer.h"
#include "WiFi_WPS.h"

void MenuRenderer::begin(TFTs *tfts, Clock *uclock, Backlights *backlights, StoredConfig *config)
{
  _tfts = tfts;
  _uclock = uclock;
  _backlights = backlights;
  _config = config;
}

void MenuRenderer::loop(Menu &menu)
{
  if (!menu.stateChanged() || !_tfts->isEnabled())
    return;

  Menu::states menu_state = menu.getState();
  int8_t menu_change = menu.getChange();

  if (menu_state == Menu::idle)
  {
    // Exiting menu: force redraw and save config
    updateClockDisplay(TFTs::force);
    Serial.println();
    Serial.print("Saving config! Triggered from leaving menu...");
    _config->save();
    Serial.println(" Done.");
    return;
  }

  switch (menu_state)
  {
  case Menu::backlight_pattern:
    renderBacklightPattern(menu_change);
    break;
  case Menu::pattern_color:
    renderPatternColor(menu_change);
    break;
  case Menu::backlight_intensity:
    renderBacklightIntensity(menu_change);
    break;
  case Menu::twelve_hour:
    renderTwelveHour(menu_change);
    break;
  case Menu::blank_hours_zero:
    renderBlankHoursZero(menu_change);
    break;
  case Menu::utc_offset_hour:
    renderUtcOffsetHour(menu_change);
    break;
  case Menu::utc_offset_15m:
    renderUtcOffset15m(menu_change);
    break;
  case Menu::selected_graphic:
    renderSelectedGraphic(menu_change);
    break;
#ifdef WIFI_USE_WPS
  case Menu::start_wps:
    renderStartWps(menu_change);
    break;
#endif
  default:
    break;
  }
}

void MenuRenderer::setupMenuDisplay()
{
  _tfts->chip_select.setHoursTens();
  _tfts->setTextColor(TFT_WHITE, TFT_BLACK);
#ifdef DISPLAY_SMALL
  _tfts->fillRect(0, 60, 80, 60, TFT_BLACK);
  _tfts->setCursor(0, 62, 2);
#else
  _tfts->fillRect(0, 120, 135, 120, TFT_BLACK);
  _tfts->setCursor(0, 124, 4);
#endif
}

void MenuRenderer::updateClockDisplay(TFTs::show_t show)
{
  _tfts->setDigit(SECONDS_ONES, _uclock->getSecondsOnes(), show);
  _tfts->setDigit(SECONDS_TENS, _uclock->getSecondsTens(), show);
  _tfts->setDigit(MINUTES_ONES, _uclock->getMinutesOnes(), show);
  _tfts->setDigit(MINUTES_TENS, _uclock->getMinutesTens(), show);
  _tfts->setDigit(HOURS_ONES, _uclock->getHoursOnes(), show);
  _tfts->setDigit(HOURS_TENS, _uclock->getHoursTens(), show);
}

void MenuRenderer::renderBacklightPattern(int8_t change)
{
  if (change != 0)
    _backlights->setNextPattern(change);
  setupMenuDisplay();
  _tfts->println("Pattern:");
  _tfts->println(_backlights->getPatternStr());
}

void MenuRenderer::renderPatternColor(int8_t change)
{
  if (change != 0)
    _backlights->adjustColorPhase(change * 16);
  setupMenuDisplay();
  _tfts->println("Color:");
  _tfts->printf("%06X\n", _backlights->getColor());
}

void MenuRenderer::renderBacklightIntensity(int8_t change)
{
  if (change != 0)
    _backlights->adjustIntensity(change);
  setupMenuDisplay();
  _tfts->println("Intensity:");
  _tfts->println(_backlights->getIntensity());
}

void MenuRenderer::renderTwelveHour(int8_t change)
{
  if (change != 0)
  {
    _uclock->toggleTwelveHour();
    _tfts->setDigit(HOURS_TENS, _uclock->getHoursTens(), TFTs::force);
    _tfts->setDigit(HOURS_ONES, _uclock->getHoursOnes(), TFTs::force);
  }
  setupMenuDisplay();
  _tfts->println("Hour format");
  _tfts->println(_uclock->getTwelveHour() ? "12 hour" : "24 hour");
}

void MenuRenderer::renderBlankHoursZero(int8_t change)
{
  if (change != 0)
  {
    _uclock->toggleBlankHoursZero();
    _tfts->setDigit(HOURS_TENS, _uclock->getHoursTens(), TFTs::force);
  }
  setupMenuDisplay();
  _tfts->println("Blank zero?");
  _tfts->println(_uclock->getBlankHoursZero() ? "yes" : "no");
}

void MenuRenderer::renderUtcOffsetHour(int8_t change)
{
  time_t currOffset = _uclock->getTimeZoneOffset();

  if (change != 0)
  {
    time_t newOffset = currOffset + (change * 3600);

    bool offsetWrapAround = false;
    if (newOffset > 43200)
    {
      newOffset = -43200;
      offsetWrapAround = true;
    }
    if (newOffset < -43200 && !offsetWrapAround)
    {
      newOffset = 43200;
    }

    _uclock->setTimeZoneOffset(newOffset);
    _uclock->loop();
#ifdef DIMMING
    // Dimming check will be handled by main loop
#endif
    currOffset = _uclock->getTimeZoneOffset();
  }

  setupMenuDisplay();
  _tfts->println("UTC Offset");
  _tfts->println(" +/- Hour");

  char offsetStr[11];
  int8_t offset_hour = currOffset / 3600;
  int8_t offset_min = (currOffset % 3600) / 60;
  if (offset_min <= 0 && offset_hour <= 0)
  {
    offset_min = -offset_min;
    offset_hour = -offset_hour;
    snprintf(offsetStr, sizeof(offsetStr), "-%d:%02d", offset_hour, offset_min);
  }
  else if (offset_min >= 0 && offset_hour >= 0)
  {
    snprintf(offsetStr, sizeof(offsetStr), "+%d:%02d", offset_hour, offset_min);
  }
  if (offset_min == 0 && offset_hour == 0)
  {
    snprintf(offsetStr, sizeof(offsetStr), "%d:%02d", offset_hour, offset_min);
  }
  _tfts->println(offsetStr);
}

void MenuRenderer::renderUtcOffset15m(int8_t change)
{
  time_t currOffset = _uclock->getTimeZoneOffset();

  if (change != 0)
  {
    time_t newOffset = currOffset + (change * 900);

    bool offsetWrapAround = false;
    if (newOffset > 43200)
    {
      newOffset = -43200;
      offsetWrapAround = true;
    }
    if (newOffset < -43200 && !offsetWrapAround)
    {
      newOffset = 43200;
    }

    _uclock->setTimeZoneOffset(newOffset);
    _uclock->loop();
#ifdef DIMMING
    // Dimming check will be handled by main loop
#endif
    currOffset = _uclock->getTimeZoneOffset();
  }

  setupMenuDisplay();
  _tfts->println("UTC Offset");
  _tfts->println(" +/- 15m");

  char offsetStr[11];
  int8_t offset_hour = currOffset / 3600;
  int8_t offset_min = (currOffset % 3600) / 60;
  if (offset_min <= 0 && offset_hour <= 0)
  {
    offset_min = -offset_min;
    offset_hour = -offset_hour;
    snprintf(offsetStr, sizeof(offsetStr), "-%d:%02d", offset_hour, offset_min);
  }
  else if (offset_min >= 0 && offset_hour >= 0)
  {
    snprintf(offsetStr, sizeof(offsetStr), "+%d:%02d", offset_hour, offset_min);
  }
  if (offset_min == 0 && offset_hour == 0)
  {
    snprintf(offsetStr, sizeof(offsetStr), "%d:%02d", offset_hour, offset_min);
  }
  _tfts->println(offsetStr);
}

void MenuRenderer::renderSelectedGraphic(int8_t change)
{
  if (change != 0)
  {
    _uclock->adjustClockGraphicsIdx(change);
    if (_tfts->current_graphic != _uclock->getActiveGraphicIdx())
    {
      _tfts->current_graphic = _uclock->getActiveGraphicIdx();
      updateClockDisplay(TFTs::force);
    }
  }
  setupMenuDisplay();
  _tfts->println("Selected");
  _tfts->println("graphic:");
  _tfts->printf("    %d\n", _uclock->getActiveGraphicIdx());
}

#ifdef WIFI_USE_WPS
void MenuRenderer::renderStartWps(int8_t change)
{
  if (change != 0)
  {
    if (change < 0)
    {
      Serial.println("WiFi WPS start request");
      _tfts->clear();
      _tfts->fillScreen(TFT_BLACK);
      _tfts->setTextColor(TFT_WHITE, TFT_BLACK);
#ifdef DISPLAY_SMALL
      _tfts->setCursor(0, 0, 1);
#else
      _tfts->setCursor(0, 0, 4);
#endif
      WiFiStartWps();
    }
  }
  setupMenuDisplay();
  _tfts->println("Connect to WiFi?");
  _tfts->println("Left=WPS");
}
#endif
