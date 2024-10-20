/*
 * Author: Aljaz Ogrin
 * Project: Alternative firmware for EleksTube IPS clock
 * Original location: https://github.com/aly-fly/EleksTubeHAX
 * Hardware: ESP32
 * Based on: https://github.com/SmittyHalibut/EleksTubeHAX
 */

#include <stdint.h>
#include "GLOBAL_DEFINES.h"
#include "Buttons.h"
#include "Backlights.h"
#include "TFTs.h"
#include "Clock.h"
#include "Menu.h"
#include "StoredConfig.h"
#include "WiFi_WPS.h"
#include "Mqtt_client_ips.h"
#include "TempSensor_inc.h"
#ifdef HARDWARE_NovelLife_SE_CLOCK // NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// #include "Gestures.h"
//TODO put into class
#include <Wire.h>
#include <SparkFun_APDS9960.h>
#endif //NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

// Constants

// Global Variables
#ifdef HARDWARE_NovelLife_SE_CLOCK // NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
//TODO put into class
SparkFun_APDS9960 apds      = SparkFun_APDS9960();
//interupt signal for gesture sensor
int volatile      isr_flag  = 0;
#endif //NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

Backlights    backlights;
Buttons       buttons;
TFTs          tfts;
Clock         uclock;
Menu          menu;
StoredConfig  stored_config;

bool          FullHour        = false;
uint8_t       hour_old        = 255;
bool          DstNeedsUpdate  = false;
uint8_t       yesterday       = 0;

uint32_t lastMqttCommandExecuted = (uint32_t) -1;

// Helper function, defined below.
void updateClockDisplay(TFTs::show_t show=TFTs::yes);
void setupMenu(void);
void EveryFullHour(bool loopUpdate=false);
void UpdateDstEveryNight(void);
#ifdef HARDWARE_NovelLife_SE_CLOCK // NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
void GestureStart();
void HandleGestureInterupt(void); //only for NovelLife SE
void GestureInterruptRoutine(void); //only for NovelLife SE
void HandleGesture(void); //only for NovelLife SE
#endif //NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

void setup() {
  Serial.begin(115200);
  delay(1000);  // Waiting for serial monitor to catch up.
  Serial.println("");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("In setup().");  

  stored_config.begin();
  stored_config.load();

  backlights.begin(&stored_config.config.backlights);
  buttons.begin();
  menu.begin();

  // Setup the displays (TFTs) initaly and show bootup message(s)
  tfts.begin();
  tfts.fillScreen(TFT_BLACK);
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.setCursor(0, 0, 2);  // Font 2. 16 pixel high
  tfts.println("Starting Setup...");

#ifdef HARDWARE_NovelLife_SE_CLOCK // NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  //Init the Gesture sensor
  tfts.setTextColor(TFT_ORANGE, TFT_BLACK);
  tfts.print("Gest start..."); Serial.print("Gesture Sensor start...");
  GestureStart(); //TODO put into class
  tfts.println("Done!"); Serial.println("Done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
#endif

  // Setup WiFi connection. Must be done before setting up Clock.
  // This is done outside Clock so the network can be used for other things.
  tfts.setTextColor(TFT_DARKGREEN, TFT_BLACK);
  tfts.println("WiFi start..."); Serial.println("WiFi start...");
  WifiBegin();
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

  // wait a bit (5x100ms = 0.5 sec) before querying NTP
  for (uint8_t ndx=0; ndx < 5; ndx++) {
    tfts.print(">");
    delay(100);
  }
  tfts.println();

  // Setup the clock.  It needs WiFi to be established already.
  tfts.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tfts.print("Clock start..."); Serial.print("Clock start...");
  uclock.begin(&stored_config.config.uclock);
  tfts.println("Done!"); Serial.println("Done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

  // Setup MQTT
  tfts.setTextColor(TFT_YELLOW, TFT_BLACK);
  tfts.print("MQTT start..."); Serial.print("MQTT start...");
  MqttStart();
  tfts.println("Done!"); Serial.println("Done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

#ifdef GEOLOCATION_ENABLED
  tfts.setTextColor(TFT_NAVY, TFT_BLACK);
  tfts.println("GeoLoc query..."); Serial.println("GeoLoc query...");
  if (GetGeoLocationTimeZoneOffset()) {
    tfts.print("TZ: "); Serial.print("TZ: ");
    tfts.println(GeoLocTZoffset); Serial.println(GeoLocTZoffset);
    uclock.setTimeZoneOffset(GeoLocTZoffset * 3600);
    Serial.println(); Serial.print("Saving config! Triggerd by timezone change...");
    stored_config.save();
    Serial.println("Done.");
    tfts.println("Done!"); Serial.println("Done!");
    tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  } else {
    tfts.setTextColor(TFT_RED, TFT_BLACK);
    tfts.println("GeoLoc FAILED"); Serial.println("GeoLoc failed!");
    tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  }
#endif

  if (uclock.getActiveGraphicIdx() > tfts.NumberOfClockFaces) {
    uclock.setActiveGraphicIdx(tfts.NumberOfClockFaces);
    Serial.println("Last selected index of clock face is larger than currently available number of image sets.");
  }
  if (uclock.getActiveGraphicIdx() < 1) {
    uclock.setActiveGraphicIdx(1);
    Serial.println("Last selected index of clock face is less than 1.");
  }
  tfts.current_graphic = uclock.getActiveGraphicIdx();

  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.println("Done with Setup!"); Serial.println("Done with Setup!");

  // Leave boot up messages on screen for a few seconds (10x200ms = 2 sec)
  for (uint8_t ndx=0; ndx < 10; ndx++) {
    tfts.print(">");
    delay(200);
  }

  // Start up the clock displays.
  tfts.fillScreen(TFT_BLACK);
  uclock.loop();
  updateClockDisplay(TFTs::force); // Draw all the clock digits
  Serial.println("Setup finished.");
}

void loop() {
  uint32_t millis_at_top = millis();
  // Do all the maintenance work
  WifiReconnect(); // if not connected attempt to reconnect
  
  MqttLoopFrequently();

  bool MqttCommandReceived = 
    MqttCommandPowerReceived || 
    MqttCommandMainPowerReceived ||
    MqttCommandBackPowerReceived ||
    MqttCommandStateReceived ||
    MqttCommandBrightnessReceived ||
    MqttCommandMainBrightnessReceived ||
    MqttCommandBackBrightnessReceived ||
    MqttCommandPatternReceived ||
    MqttCommandBackPatternReceived ||
    MqttCommandBackColorPhaseReceived || 
    MqttCommandGraphicReceived ||
    MqttCommandMainGraphicReceived ||
    MqttCommandUseTwelveHoursReceived ||
    MqttCommandBlankZeroHoursReceived ||
    MqttCommandPulseBpmReceived ||
    MqttCommandBreathBpmReceived ||
    MqttCommandRainbowSecReceived;
  
  if (MqttCommandPowerReceived) {
    MqttCommandPowerReceived = false;
    if (MqttCommandPower) {
#ifdef HARDWARE_Elekstube_CLOCK // maybe also for other clocks needed? => check the direct clones! PunkCyber needs it too.
      // Needed for original EleksTube hardware only, because after a few hours in OFF state, the displays do not wake up properly
      // -> so reinit them (includes enabling the displays)
      tfts.reinit();
#else
      // for all other clocks, just enable the displays
      tfts.enableAllDisplays();
#endif
      //redraw all the clock digits -> needed because the displays was blanked before turning off
      updateClockDisplay(TFTs::force);
      backlights.PowerOn();
    } else {
      // blank the screens before turning off -> needed for all clocks without a real "power switch curcuit"
      // to "simulate" the off-switched displays
      tfts.chip_select.setAll();
      tfts.fillScreen(TFT_BLACK);
      tfts.disableAllDisplays();
      backlights.PowerOff();
    }
  }

  if (MqttCommandMainPowerReceived) {
    MqttCommandMainPowerReceived = false;
    if (MqttCommandMainPower) {
#ifdef HARDWARE_Elekstube_CLOCK // maybe also for other clocks needed? => check the direct clones! PunkCyber needs it too.
      // Needed for original EleksTube hardware only, because after a few hours in OFF state, the displays do not wake up properly
      // -> so reinit them (includes enabling the displays)
      tfts.reinit();
#else
      // for all other clocks, just enable the displays
      tfts.enableAllDisplays();
#endif
      //redraw all the clock digits -> needed because the displays was blanked before turning off
      updateClockDisplay(TFTs::force);
    } else {
      // blank the screens before turning off -> needed for all clocks without a real "power switch curcuit"
      // to "simulate" the off-switched displays
      tfts.chip_select.setAll();
      tfts.fillScreen(TFT_BLACK);
      tfts.disableAllDisplays();
    }
  }

  if (MqttCommandBackPowerReceived) {
    MqttCommandBackPowerReceived = false;
    if (MqttCommandBackPower) {
      backlights.PowerOn();
    } else {
      backlights.PowerOff();
    }
  }

  if (MqttCommandStateReceived) {
    MqttCommandStateReceived = false;
    randomSeed(millis());
    uint8_t idx;
    if (MqttCommandState >= 90)
      { idx = random(1, tfts.NumberOfClockFaces+1); }
    else
      { idx = (MqttCommandState / 5) -1; }  // 10..40 -> graphic 1..6
    DBG_MQTT("Graphic change request from MQTT; command: ");
    DBG_MQTT(MqttCommandState);
    DBG_MQTT(", index: ");
    DBG_MQTT_L(idx);
    uclock.setClockGraphicsIdx(idx);  
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    updateClockDisplay(TFTs::force);   // redraw all the clock digits
  }

  if(MqttCommandMainBrightnessReceived) {
    MqttCommandMainBrightnessReceived = false;
    tfts.dimming = MqttCommandMainBrightness;
    tfts.ProcessUpdatedDimming();
    updateClockDisplay(TFTs::force); // redraw all the clock digits
  }

  if(MqttCommandBackBrightnessReceived) {
    MqttCommandBackBrightnessReceived = false;
    backlights.setIntensity(uint8_t(MqttCommandBackBrightness));
  }

  if(MqttCommandPatternReceived) {
    MqttCommandPatternReceived = false;

    for(int8_t i = 0; i < Backlights::num_patterns; i++){
        DBG_MQTT("New pattern ");
        DBG_MQTT(MqttCommandPattern);
        DBG_MQTT(", check pattern ");
        DBG_MQTT_L(Backlights::patterns_str[i]);
      if(strcmp(MqttCommandPattern, (Backlights::patterns_str[i]).c_str()) == 0) {
        backlights.setPattern(Backlights::patterns(i));
        break;
      }
    } 
  }

  if(MqttCommandBackPatternReceived) {
    MqttCommandBackPatternReceived = false;
    for(int8_t i = 0; i < Backlights::num_patterns; i++){
        DBG_MQTT("new pattern ");
        DBG_MQTT(MqttCommandBackPattern);
        DBG_MQTT(", check pattern ");
        DBG_MQTT_L(Backlights::patterns_str[i]);
      if(strcmp(MqttCommandBackPattern, (Backlights::patterns_str[i]).c_str()) == 0) {
        backlights.setPattern(Backlights::patterns(i));
        break;
      }
    } 
  }

  if(MqttCommandBackColorPhaseReceived) {
    MqttCommandBackColorPhaseReceived = false;
    
    backlights.setColorPhase(MqttCommandBackColorPhase);
  } 

  if(MqttCommandGraphicReceived) {
    MqttCommandGraphicReceived = false;

    uclock.setClockGraphicsIdx(MqttCommandGraphic);
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    updateClockDisplay(TFTs::force); // redraw all the clock digits
  }

  if(MqttCommandMainGraphicReceived) {
    MqttCommandMainGraphicReceived = false;
    uclock.setClockGraphicsIdx(MqttCommandMainGraphic);
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    updateClockDisplay(TFTs::force); // redraw all the clock digits
  }

  if(MqttCommandUseTwelveHoursReceived) {
    MqttCommandUseTwelveHoursReceived = false;
    uclock.setTwelveHour(MqttCommandUseTwelveHours);
  }

  if(MqttCommandBlankZeroHoursReceived) {
    MqttCommandBlankZeroHoursReceived = false;
    uclock.setBlankHoursZero(MqttCommandBlankZeroHours);
  }

  if(MqttCommandPulseBpmReceived) {
    MqttCommandPulseBpmReceived = false;
    backlights.setPulseRate(MqttCommandPulseBpm);
  }

  if(MqttCommandBreathBpmReceived) {
    MqttCommandBreathBpmReceived = false;
    backlights.setBreathRate(MqttCommandBreathBpm);
  }

  if(MqttCommandRainbowSecReceived) {
    MqttCommandRainbowSecReceived = false;
    backlights.setRainbowDuration(MqttCommandRainbowSec);
  }

  MqttStatusPower = tfts.isEnabled();
  MqttStatusMainPower = tfts.isEnabled();
  MqttStatusBackPower = backlights.getPower();
  MqttStatusState = (uclock.getActiveGraphicIdx()+1) * 5;   // 10 
  MqttStatusBrightness = backlights.getIntensity();
  MqttStatusMainBrightness = tfts.dimming;
  MqttStatusBackBrightness = backlights.getIntensity();
  strcpy(MqttStatusPattern, backlights.getPatternStr().c_str());
  strcpy(MqttStatusBackPattern, backlights.getPatternStr().c_str());
  backlights.getPatternStr().toCharArray(MqttStatusBackPattern, backlights.getPatternStr().length() + 1);
  MqttStatusBackColorPhase = backlights.getColorPhase();
  MqttStatusGraphic = uclock.getActiveGraphicIdx();
  MqttStatusMainGraphic = uclock.getActiveGraphicIdx();
  MqttStatusUseTwelveHours = uclock.getTwelveHour();
  MqttStatusBlankZeroHours = uclock.getBlankHoursZero();
  MqttStatusPulseBpm = backlights.getPulseRate();
  MqttStatusBreathBpm = backlights.getBreathRate();
  MqttStatusRainbowSec = backlights.getRainbowDuration();

  if(MqttCommandReceived) {
    lastMqttCommandExecuted = millis();
    MqttReportBackEverything(true);
  }

  //Auto store preferences - every 60 secs after the last MQTT command was received
  if(lastMqttCommandExecuted != -1) {
    if (((millis() - lastMqttCommandExecuted) > (MQTT_SAVE_PREFERENCES_AFTER_SEC * 1000)) && menu.getState() == Menu::idle) {
      lastMqttCommandExecuted = -1;
      DBG_MQTT_L(""); DBG_MQTT("Saving config! Triggered from MQTT command received...");
      stored_config.save();
      DBG_MQTT_L("Done!");
    }
  }

  buttons.loop();

#ifdef HARDWARE_NovelLife_SE_CLOCK // NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  HandleGestureInterupt();
#endif // NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

  // Power button was pressed: if in the menu, exit menu, else turn off displays and backlight.
#ifndef ONE_BUTTON_ONLY_MENU
  if (buttons.power.isUpEdge() && (menu.getState() == Menu::idle)) {
    // check, if we need to switch off the LC displays and LED backlights
    if (tfts.isEnabled()) {
      // state is enabled -> turn OFF the displays and backlights

      // blank the screens before turning off -> needed for all clocks without a real "power switch curcuit"
      // to "simulate" the off-switched displays
      tfts.chip_select.setAll();
      tfts.fillScreen(TFT_BLACK);
      tfts.disableAllDisplays();
      backlights.PowerOff();
    }
    else {
      // state is disabled -> turn ON the displays and backlights
#ifdef HARDWARE_Elekstube_CLOCK // maybe also for other clocks needed? => check the direct clones! PunkCyber needs it too.
      // Needed for original EleksTube hardware only, because after a few hours in OFF state, the displays do not wake up properly
      // -> so reinit them (includes enabling the displays)
      tfts.reinit();
#else
      // for all other clocks, just enable the displays
      tfts.enableAllDisplays();
#endif
      //redraw all the clock digits -> needed because the displays was blanked before turning off
      updateClockDisplay(TFTs::force);
      backlights.PowerOn();
    }
  }
#endif // ONE_BUTTON_ONLY_MENU

 
  menu.loop(buttons);  // Must be called after buttons.loop()
  backlights.loop();
  uclock.loop();

  EveryFullHour(true); // night or daytime

  updateClockDisplay(); // Draw only the changed clock digits!
  
  UpdateDstEveryNight();

  // Menu
  if (menu.stateChanged() && tfts.isEnabled()) {
    Menu::states menu_state = menu.getState();
    int8_t menu_change = menu.getChange();

    if (menu_state == Menu::idle) {
      // We just changed into idle, so force a redraw of all clock digits and save the config.
      updateClockDisplay(TFTs::force); // redraw all the clock digits
      Serial.println(); Serial.print("Saving config! Triggered from leaving menu...");
      stored_config.save();
      Serial.println(" Done.");
    }
    else {
      // Backlight Pattern
      if (menu_state == Menu::backlight_pattern) {
        if (menu_change != 0) {
          backlights.setNextPattern(menu_change);
        }
        setupMenu();
        tfts.println("Pattern:");
        tfts.println(backlights.getPatternStr());
      }
      // Backlight Color
      else if (menu_state == Menu::pattern_color) {
        if (menu_change != 0) {
          backlights.adjustColorPhase(menu_change*16);
        }
        setupMenu();
        tfts.println("Color:");
        tfts.printf("%06X\n", backlights.getColor()); 
      }
      // Backlight Intensity
      else if (menu_state == Menu::backlight_intensity) {
        if (menu_change != 0) {
          backlights.adjustIntensity(menu_change);
        }
        setupMenu();
        tfts.println("Intensity:");
        tfts.println(backlights.getIntensity());
      }
      // 12 Hour or 24 Hour mode?
      else if (menu_state == Menu::twelve_hour) {
        if (menu_change != 0) {
          uclock.toggleTwelveHour();
          tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), TFTs::force);
          tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), TFTs::force);
        }        
        setupMenu();
        tfts.println("Hour format");
        tfts.println(uclock.getTwelveHour() ? "12 hour" : "24 hour"); 
      }
      // Blank leading zeros on the hours?
      else if (menu_state == Menu::blank_hours_zero) {
        if (menu_change != 0) {
          uclock.toggleBlankHoursZero();
          tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), TFTs::force);
        }        
        setupMenu();
        tfts.println("Blank zero?");
        tfts.println(uclock.getBlankHoursZero() ? "yes" : "no");
      }
      // UTC Offset, hours
      else if (menu_state == Menu::utc_offset_hour) {
        time_t currOffset = uclock.getTimeZoneOffset();

        if (menu_change != 0) {
          // calculate the new offset
          time_t newOffsetAdjustmentValue = menu_change * 3600;
          time_t newOffset = currOffset + newOffsetAdjustmentValue;          
          double newOffsetInHours = static_cast<double>(newOffset) / 3600;

          // Check if the sign of the offset has changed (passed over +/-0 hours)
          if ((currOffset < 0 && newOffset > 0) || (currOffset > 0 && newOffset < 0)) {
              newOffset = newOffsetInHours * 3600;
          }

          // If the minutes part of the offset is 0, we want to change from +12 to -12 or vice versa (without changing the shown time on the displays)
          // if the minutes part is not 0, we want to wrap around to the other side but keep the minutes part (i.e. from 11:45 directly to -11:45)
          // so we will change the shown time on the displays if the minutes part is not 0!
          // Only other choice would be to go to the next offset without a minute part (i.e. from 11:15 to 12:00 and then to -12:00) so minutes part would be lost.
          bool offsetWrapAround = false;
          // Check if the new offset is within the allowed range of -12 to +12 hours
          if (newOffset > 43200) { // overflow (12*3600 = 43200 -> set to -12 hour)
              int currentOffsetMinutesPartAsSeconds = newOffset % 3600;
              if (currentOffsetMinutesPartAsSeconds == 0) { // offset was exactly +12 hours, minutes part is 0
                  newOffset = -43200; // set to -12 hours
                  offsetWrapAround = true;
              } else {
                  newOffset = -39600 + (-currentOffsetMinutesPartAsSeconds);  // wrap around to the negative side but add the minutes part
                  offsetWrapAround = true;
              }
          }
          if (newOffset < -43200 && !offsetWrapAround) { // underflow (-12*3600 = -43200 -> set to +12 hour)
              int currentOffsetMinutesPartAsSeconds = newOffset % 3600;
              if (currentOffsetMinutesPartAsSeconds == 0) { // offset was exactly -12 hours, minutes part is 0
                  newOffset = 43200; // set to +12 hours
              } else {
                  currentOffsetMinutesPartAsSeconds = -currentOffsetMinutesPartAsSeconds; // make the minutes part positive
                  newOffset = 39600 + currentOffsetMinutesPartAsSeconds; // wrap around to the positive side but add the minutes part
              }
          }
          uclock.setTimeZoneOffset(newOffset); // set the new offset
          uclock.loop(); // update the clock time -> will "flicker" the menu for a short time, but without, menu is not redrawn at all
// TODO: check if needed!
// EveryFullHour(); // check if we need dimming for the night, because timezone was changed
          tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), TFTs::yes);
          tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), TFTs::yes);
          currOffset = uclock.getTimeZoneOffset(); // get the new offset as current offset for the menu
        }
        setupMenu();
        tfts.println("UTC Offset");
        tfts.println(" +/- Hour");
        char offsetStr[11];
        int8_t offset_hour = currOffset/3600;
        int8_t offset_min = (currOffset%3600)/60;
        if(offset_min <= 0 && offset_hour <= 0) { // negative timezone value -> Make them positive and print a minus in front
          offset_min = -offset_min;
          offset_hour = -offset_hour;
          snprintf(offsetStr, sizeof(offsetStr), "-%d:%02d", offset_hour, offset_min);
        } else {
          if(offset_min >= 0 && offset_hour >= 0) {// postive timezone value for hours and minutes -> show a plus in front
              snprintf(offsetStr, sizeof(offsetStr), "+%d:%02d", offset_hour, offset_min);
          }
        }
        if(offset_min == 0 && offset_hour == 0) { // we don't want a sign in front of the 0:00 case
            snprintf(offsetStr, sizeof(offsetStr), "%d:%02d", offset_hour, offset_min);
        }
        tfts.println(offsetStr);
      } // END UTC Offset, hours
      // BEGIN UTC Offset, 15 minutes
      else if (menu_state == Menu::utc_offset_15m) {
        time_t currOffset = uclock.getTimeZoneOffset();

        if (menu_change != 0) {
          time_t newOffsetAdjustmentValue = menu_change * 900; // calculate the new offset
          time_t newOffset = currOffset + newOffsetAdjustmentValue;

          // check if the new offset is within the allowed range of -12 to +12 hours
          // same behaviour as for the +/-1 hour offset, but with 15 minutes steps
          bool offsetWrapAround = false;
          if (newOffset > 43200) { // we just "passed" +12 hours -> set to -12 hours
            newOffset = -43200;
            offsetWrapAround = true;
          }
          if (newOffset == -43200 && !offsetWrapAround) { // we just passed -12 hours -> set to +12 hours
            newOffset = 43200;
          }
          uclock.setTimeZoneOffset(newOffset); // set the new offset
          uclock.loop(); // update the clock time -> will "flicker" the menu for a short time, but without, menu is not redrawn at all
// TODO: check if needed!
// EveryFullHour(); // check if we need dimming for the night, because timezone was changed          
          tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), TFTs::yes);
          tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), TFTs::yes);
          tfts.setDigit(MINUTES_TENS, uclock.getMinutesTens(), TFTs::yes);
          tfts.setDigit(MINUTES_ONES, uclock.getMinutesOnes(), TFTs::yes);
          currOffset = uclock.getTimeZoneOffset(); // get the new offset as current offset for the menu
        }
        setupMenu();
        tfts.println("UTC Offset");
        tfts.println(" +/- 15m");
        char offsetStr[11];
        int8_t offset_hour = currOffset/3600;
        int8_t offset_min = (currOffset%3600)/60;
        if(offset_min <= 0 && offset_hour <= 0) { // negative timezone value -> Make them positive and print a minus in front
          offset_min = -offset_min;
          offset_hour = -offset_hour;
          snprintf(offsetStr, sizeof(offsetStr), "-%d:%02d", offset_hour, offset_min);
        } else {
          if(offset_min >= 0 && offset_hour >= 0) {// postive timezone value for hours and minutes -> show a plus in front
              snprintf(offsetStr, sizeof(offsetStr), "+%d:%02d", offset_hour, offset_min);
          }
        }
        if(offset_min == 0 && offset_hour == 0) { // we don't want a sign in front of the 0:00 case so overwrite the string
            snprintf(offsetStr, sizeof(offsetStr), "%d:%02d", offset_hour, offset_min);
        }
        tfts.println(offsetStr);
      } // END UTC Offset, 15 minutes
      // BGEIN select clock face
      else if (menu_state == Menu::selected_graphic) {
        if (menu_change != 0) {
          uclock.adjustClockGraphicsIdx(menu_change);

          if(tfts.current_graphic != uclock.getActiveGraphicIdx()) {
            tfts.current_graphic = uclock.getActiveGraphicIdx();
            updateClockDisplay(TFTs::force); // redraw all the clock digits
          }
        }
        setupMenu();
        tfts.println("Selected");
        tfts.println("graphic:");
        tfts.printf("    %d\n", uclock.getActiveGraphicIdx());
      }
#ifdef WIFI_USE_WPS   //  WPS code
      // connect to WiFi using wps pushbutton mode
      else if (menu_state == Menu::start_wps) {
        if (menu_change != 0) { // button was pressed
          if (menu_change < 0) { // left button
            Serial.println("WiFi WPS start request");
            tfts.clear();
            tfts.fillScreen(TFT_BLACK);
            tfts.setTextColor(TFT_WHITE, TFT_BLACK);
            tfts.setCursor(0, 0, 4);  // Font 4. 26 pixel high
            WiFiStartWps();
          }
        }        
        setupMenu();
        tfts.println("Connect to WiFi?");
        tfts.println("Left=WPS");
      }
#endif
    }
  } // if (menu.stateChanged())

  uint32_t time_in_loop = millis() - millis_at_top;
  if (time_in_loop < 20) {
    // we have free time, spend it for loading next image into buffer
    tfts.LoadNextImage();

    // we still have extra time
    time_in_loop = millis() - millis_at_top;
    if (time_in_loop < 20) {
      MqttLoopInFreeTime();
#ifdef ONE_WIRE_BUS_PIN      
      PeriodicReadTemperature();
      if (bTemperatureUpdated) {
        tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), TFTs::force);  // show latest clock digit and temperature readout together
        bTemperatureUpdated = false;
      }
#endif
      
      // run once a day (= 744 times per month which is below the limit of 5k for free account)
      if (DstNeedsUpdate) { // Daylight savings time changes at 3 in the morning
        if (GetGeoLocationTimeZoneOffset()) {
          uclock.setTimeZoneOffset(GeoLocTZoffset * 3600);
          DstNeedsUpdate = false;  // done for this night; retry if not sucessfull
        }
      }  
      // Sleep for up to 20ms, less if we've spent time doing stuff above.
      time_in_loop = millis() - millis_at_top;
      if (time_in_loop < 20) {
        delay(20 - time_in_loop);
      }
    }
  }
// #ifdef DEBUG_OUTPUT
//   if (time_in_loop <= 2) Serial.print(".");
//   else {
//     Serial.print("time spent in loop (ms): ");
//     Serial.println(time_in_loop);
//   }
// #endif
}
#ifdef HARDWARE_NovelLife_SE_CLOCK // NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
void GestureStart()
{
    // for gesture sensor APDS9660 - Set interrupt pin on ESP32 as input
  pinMode(GESTURE_SENSOR_INPUT_PIN, INPUT);

  // Initialize interrupt service routine for interupt from APDS-9960 sensor
  attachInterrupt(digitalPinToInterrupt(GESTURE_SENSOR_INPUT_PIN), GestureInterruptRoutine, FALLING);

  // Initialize gesture sensor APDS-9960 (configure I2C and initial values)
  if ( apds.init() ) {
    Serial.println(F("APDS-9960 initialization complete"));

    //Set Gain to 1x, bacause the cheap chinese fake APDS sensor can't handle more (also remember to extend ID check in Sparkfun libary to 0x3B!)
    apds.setGestureGain(GGAIN_1X);
          
    // Start running the APDS-9960 gesture sensor engine
    if ( apds.enableGestureSensor(true) ) {
      Serial.println(F("Gesture sensor is now running"));
    } else {
      Serial.println(F("Something went wrong during gesture sensor enablimg in the APDS-9960 library!"));
    }
  } else {
    Serial.println(F("Something went wrong during APDS-9960 init!"));
  }
}

//Handle Interrupt from gesture sensor and simulate a short button press of the corresponding button, if a gesture is detected 
void HandleGestureInterupt()
{
  if( isr_flag == 1 ) {
    detachInterrupt(digitalPinToInterrupt(GESTURE_SENSOR_INPUT_PIN));
    HandleGesture();
    isr_flag = 0;
    attachInterrupt(digitalPinToInterrupt(GESTURE_SENSOR_INPUT_PIN), GestureInterruptRoutine, FALLING);
  }
  return;
}

//mark, that the Interrupt of the gesture sensor was signaled
void GestureInterruptRoutine() {
  isr_flag = 1;
  return;
}

//check which gesture was detected
void HandleGesture() {
  if ( apds.isGestureAvailable() ) {
    switch ( apds.readGesture() ) {
      case DIR_UP:
        buttons.left.setUpEdgeState();
        Serial.println(); Serial.println("Gesture detected! LEFT");
        break;
      case DIR_DOWN:
        buttons.right.setUpEdgeState();
        Serial.println(); Serial.println("Gesture detected! RIGHT");
        break;
      case DIR_LEFT:
        buttons.power.setUpEdgeState();
        Serial.println(); Serial.println("Gesture detected! DOWN");
        break;
      case DIR_RIGHT:
        buttons.mode.setUpEdgeState();
        Serial.println(); Serial.println("Gesture detected! UP");
        break;
      case DIR_NEAR:
        buttons.mode.setUpEdgeState();
        Serial.println(); Serial.println("Gesture detected! NEAR");
        break;
      case DIR_FAR:
        buttons.power.setUpEdgeState();
        Serial.println(); Serial.println("Gesture detected! FAR");
        break;
      default:
        Serial.println(); Serial.println("Movement detected but NO gesture detected!");
    }
  }
  return;
}
#endif // NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

void setupMenu() {
  // Prepare drawing of the menu texts

  // use most left display
  tfts.chip_select.setHoursTens();
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  //use lower half of the display, fill with black
  tfts.fillRect(0, 120, 135, 120, TFT_BLACK);
  // use font 4. 26 pixel high
  tfts.setCursor(0, 124, 4);  // Font 4. 26 pixel high
}

bool isNightTime(uint8_t current_hour) {
    if (DAY_TIME < NIGHT_TIME) {
      // "Night" spans across midnight
      return (current_hour < DAY_TIME) || (current_hour >= NIGHT_TIME);
    }
    else {
      // "Night" starts after midnight, entirely contained within the day
      return (current_hour >= NIGHT_TIME) && (current_hour < DAY_TIME);
    }
}

void EveryFullHour(bool loopUpdate) {
  // dim the clock in the night
#ifdef NIGHTTIME_DIMMING
  uint8_t current_hour = uclock.getHour24();
  FullHour = current_hour != hour_old;
  if (FullHour) {
    Serial.print("current hour = ");
    Serial.println(current_hour);
    if (isNightTime(current_hour)) {
      Serial.println("Setting night mode (dimmed)!");
      tfts.dimming = TFT_DIMMED_INTENSITY;
      tfts.ProcessUpdatedDimming();
      backlights.setDimming(true);
      if (menu.getState() == Menu::idle || (loopUpdate==true)) { // otherwise erases the menu
        updateClockDisplay(TFTs::force); // redraw all the clock digits
      }
    } else {
      Serial.println("Setting daytime mode (max brightness)!");
      tfts.dimming = 255; // 0..255
      tfts.ProcessUpdatedDimming();
      backlights.setDimming(false);
      if (menu.getState() == Menu::idle || (loopUpdate==true)) { // otherwise erases the menu
        updateClockDisplay(TFTs::force); // redraw all the clock digits
      }
    }
    hour_old = current_hour;
  }
#endif
}

void UpdateDstEveryNight() {
  uint8_t currentDay = uclock.getDay();
  // This `DstNeedsUpdate` is True between 3:00:05 and 3:00:59. Has almost one minute of time slot to fetch updates, incl. eventual retries.
  DstNeedsUpdate = (currentDay != yesterday) && (uclock.getHour24() == 3) && (uclock.getMinute() == 0) && (uclock.getSecond() > 5);
  if (DstNeedsUpdate) {
  Serial.print("DST needs update...");

  // Update day after geoloc was sucesfully updated. Otherwise this will immediatelly disable the failed update retry.
  yesterday = currentDay;
  }
}

void updateClockDisplay(TFTs::show_t show) {
  // refresh starting on seconds
  tfts.setDigit(SECONDS_ONES, uclock.getSecondsOnes(), show);
  tfts.setDigit(SECONDS_TENS, uclock.getSecondsTens(), show);
  tfts.setDigit(MINUTES_ONES, uclock.getMinutesOnes(), show);
  tfts.setDigit(MINUTES_TENS, uclock.getMinutesTens(), show);
  tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), show);
  tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), show);
}
