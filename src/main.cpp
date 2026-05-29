/*
 * Author: Aljaz Ogrin
 * Project: Alternative firmware for EleksTube IPS clock
 * Original location: https://github.com/aly-fly/EleksTubeHAX
 * Hardware: ESP32
 * Based on: https://github.com/SmittyHalibut/EleksTubeHAX
 *
 * Refactored: Logic extracted into dedicated classes for readability.
 */

#include <nvs_flash.h>
#include <stdint.h>
#include <math.h>
#include <Wire.h>
#include <SPI.h>
#include "GLOBAL_DEFINES.h"
#include "Backlights.h"
#include "Buttons.h"
#include "Clock.h"
#include "Menu.h"
#include "StoredConfig.h"
#include "TFTs.h"
#include "WiFi_WPS.h"

// Extracted modules
#include "GestureHandler.h"
#include "MQTTCommandProcessor.h"
#include "MenuRenderer.h"
#include "GeolocationManager.h"
#include "DimmingManager.h"
#include "SerialCommandHandler.h"
#include "WebOTAManager.h"

// ─── Global hardware objects ────────────────────────────────────────────────
char UniqueDeviceName[32];

Backlights backlights;
Buttons buttons;
TFTs tfts;
Clock uclock;
Menu menu;
StoredConfig stored_config;

// ─── Extracted module instances ─────────────────────────────────────────────
GestureHandler gestureHandler;
MQTTCommandProcessor mqttProcessor;
MenuRenderer menuRenderer;
GeolocationManager geolocation;
DimmingManager dimming;
SerialCommandHandler serialCmd;
WebOTAManager webOta;

// ─── Helper function ────────────────────────────────────────────────────────
void updateClockDisplay(TFTs::show_t show = TFTs::yes);

//-----------------------------------------------------------------------
// Setup
//-----------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println("\nSystem starting...\n");
  Serial.println("EleksTubeHAX https://github.com/aly-fly/EleksTubeHAX");
  Serial.printf("Firmware version: v%s.\n", FIRMWARE_VERSION);

  // Prepare unique device name
#ifdef MQTT_CLIENT_ID_FOR_SMARTNEST
  snprintf(UniqueDeviceName, sizeof(UniqueDeviceName), "%s", MQTT_CLIENT_ID_FOR_SMARTNEST);
#else
  {
    uint64_t rawmac = ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL;
    uint8_t mac_bytes[6];
    for (int i = 0; i < 6; i++)
      mac_bytes[i] = (rawmac >> (8 * i)) & 0xFF;
    snprintf(UniqueDeviceName, sizeof(UniqueDeviceName), "%s-%02X%02X%02X", DEVICE_NAME,
             mac_bytes[3], mac_bytes[4], mac_bytes[5]);
  }
#endif
  // Sanitize device name for MQTT topics
  for (size_t i = 0; i < sizeof(UniqueDeviceName); ++i)
  {
    char c = UniqueDeviceName[i];
    if (c == '\0')
      break;
    c = (char)tolower((int)c);
    if (!isalnum((unsigned char)c) && c != '-' && c != '_')
      c = '_';
    UniqueDeviceName[i] = c;
  }
  Serial.printf("Set device name: \"%s\".\n", UniqueDeviceName);

  // NVS initialization
  Serial.print("Init NVS flash partition usage...");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    Serial.println("\nNo free pages in or newer version of NVS partition found. Erasing...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  Serial.println("Done.");

  // Core hardware
  stored_config.begin();
  stored_config.load();
  backlights.begin(&stored_config.config.backlights);
#ifndef NO_BUTTONS
  buttons.begin();
#endif
  menu.begin();

  // Displays
  tfts.begin();
  tfts.fillScreen(TFT_BLACK);
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
#ifdef DISPLAY_SMALL
  tfts.setCursor(0, 0, 1);
#else
  tfts.setCursor(0, 0, 2);
#endif
  tfts.println("Starting Setup...");

  // Gesture sensor (NovelLife only, no-op for other hardware)
  tfts.setTextColor(TFT_ORANGE, TFT_BLACK);
  tfts.print("Gest start...");
  Serial.println("Gesture sensor start...");
  gestureHandler.begin();
  tfts.println("Done!");
  Serial.println("Gesture sensor Done.");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

  // WiFi
  tfts.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tfts.println("WiFi start...");
  Serial.println("WiFi start...");
  WifiBegin();
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

  for (uint8_t ndx = 0; ndx < 5; ndx++)
  {
    tfts.print(">");
    delay(100);
  }
  tfts.println("");

  // Clock (needs WiFi for NTP)
  tfts.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tfts.print("Clock start...");
  Serial.println("\nClock start-up...");
  uclock.begin(&stored_config.config.uclock);
  tfts.println("Done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

  // MQTT
#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
  tfts.setTextColor(TFT_YELLOW, TFT_BLACK);
  tfts.print("MQTT start...");
  Serial.println("MQTT start...");
  MQTTStart(false);
  mqttProcessor.begin(&tfts, &backlights, &uclock, &stored_config, &menu);
  tfts.println("Done!");
  Serial.println("MQTT Done.");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
#endif

  // Geolocation
  tfts.setTextColor(TFT_CYAN, TFT_BLACK);
  tfts.println("GeoLoc query...");
  Serial.println("GeoLoc query...");
  geolocation.begin(&uclock, &stored_config);
  if (geolocation.queryTimezoneOffset())
  {
    int32_t tzOffsetH = uclock.getTimeZoneOffset() / 3600;
    tfts.print("TZ: ");
    Serial.print("TZ offset (hours): ");
    tfts.println(tzOffsetH);
    Serial.println(tzOffsetH);
    tfts.println("Done!");
    Serial.println("GeoLoc Done!");
  }
  else
  {
    tfts.setTextColor(TFT_RED, TFT_BLACK);
    tfts.println("GeoLoc FAILED");
    Serial.println("GeoLoc FAILED!");
  }
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

  // Validate clock face index
  if (uclock.getActiveGraphicIdx() > tfts.NumberOfClockFaces)
  {
    uclock.setActiveGraphicIdx(tfts.NumberOfClockFaces);
    Serial.println("Clock face index clamped to max available.");
  }
  if (uclock.getActiveGraphicIdx() < 1)
  {
    uclock.setActiveGraphicIdx(1);
    Serial.println("Clock face index clamped to 1.");
  }
  tfts.current_graphic = uclock.getActiveGraphicIdx();

  // Initialize extracted modules
  menuRenderer.begin(&tfts, &uclock, &backlights, &stored_config);
  dimming.begin(&uclock, &tfts, &backlights, &stored_config);
  serialCmd.begin(&uclock, &tfts, &backlights, &stored_config);

  // Web server & OTA
  tfts.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tfts.print("Web/OTA...");
  Serial.println("Web server & OTA start...");
  webOta.begin(&uclock, &tfts, &backlights, &stored_config, &dimming, UniqueDeviceName);
  tfts.println("Done!");
  Serial.println("Web/OTA Done.");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

  tfts.println("Done with Setup!");
  Serial.println("\nDone with Setup!");

  // Leave bootup messages on screen for 2 seconds
  for (uint8_t ndx = 0; ndx < 10; ndx++)
  {
    tfts.print(">");
    delay(200);
  }

  // Start the clock displays
  tfts.fillScreen(TFT_BLACK);
  uclock.loop();
  updateClockDisplay(TFTs::force);
  Serial.println("Starting main loop...");
}

//-----------------------------------------------------------------------
// Main loop
//-----------------------------------------------------------------------
void loop()
{
  uint32_t millis_at_top = millis();

  // ─── Connectivity ──────────────────────────────────────────────────────────
  WifiReconnect();
  WifiPortalHandle(); // Handle captive portal if active (no-op when not in AP mode)

  // ─── MQTT ──────────────────────────────────────────────────────────────────
  mqttProcessor.loop();

  // ─── Buttons & Gestures ────────────────────────────────────────────────────
#ifndef NO_BUTTONS
  buttons.loop();
#endif
  gestureHandler.handle(buttons);

  // ─── Power button ──────────────────────────────────────────────────────────
#ifndef ONE_BUTTON_ONLY_MENU
  if (buttons.power.isDownEdge() && (menu.getState() == Menu::idle))
  {
    if (tfts.isEnabled())
    {
      tfts.chip_select.setAll();
      tfts.fillScreen(TFT_BLACK);
      tfts.disableAllDisplays();
      backlights.PowerOff();
    }
    else
    {
#ifdef HARDWARE_ELEKSTUBE_CLOCK
      tfts.reinit();
#else
      tfts.enableAllDisplays();
#endif
      updateClockDisplay(TFTs::force);
      backlights.PowerOn();
    }
  }
#endif // ONE_BUTTON_ONLY_MENU

  // ─── Menu ─────────────────────────────────────────────────────────────────
  menu.loop(buttons);

#ifdef CAPACITIVE_TOUCH_BUTTONS
  if (menu.isPowerToggle())
  {
    if (tfts.isEnabled())
    {
      tfts.chip_select.setAll();
      tfts.fillScreen(TFT_BLACK);
      tfts.disableAllDisplays();
      backlights.PowerOff();
    }
    else
    {
      tfts.enableAllDisplays();
      updateClockDisplay(TFTs::force);
      backlights.PowerOn();
    }
  }
#endif

  menuRenderer.loop(menu);

  // ─── Clock & Display ───────────────────────────────────────────────────────
  backlights.loop();
  uclock.loop();

  if (dimming.check())
    updateClockDisplay(TFTs::force);

  updateClockDisplay();

  // ─── Geolocation ──────────────────────────────────────────────────────────
  geolocation.checkUpdateNeeded();

  // ─── Serial commands ──────────────────────────────────────────────────────
  serialCmd.handle();

  // ─── Web/OTA ──────────────────────────────────────────────────────────────
  webOta.handle();

  // ─── Free time handling ────────────────────────────────────────────────────
  uint32_t time_in_loop = millis() - millis_at_top;
  if (time_in_loop < 20)
  {
    tfts.LoadNextImage();

    time_in_loop = millis() - millis_at_top;
    if (time_in_loop < 20)
    {
      mqttProcessor.loopInFreeTime();
      geolocation.processUpdate();

      time_in_loop = millis() - millis_at_top;
      if (time_in_loop < 20)
        delay(20 - time_in_loop);
    }
  }

#ifdef DEBUG_OUTPUT
  if (time_in_loop <= 2)
    Serial.print(".");
  else
  {
    Serial.print("time spent in loop (ms): ");
    Serial.println(time_in_loop);
  }
#endif
}

// ─── Helper ─────────────────────────────────────────────────────────────────
void updateClockDisplay(TFTs::show_t show)
{
  tfts.setDigit(SECONDS_ONES, uclock.getSecondsOnes(), show);
  tfts.setDigit(SECONDS_TENS, uclock.getSecondsTens(), show);
  tfts.setDigit(MINUTES_ONES, uclock.getMinutesOnes(), show);
  tfts.setDigit(MINUTES_TENS, uclock.getMinutesTens(), show);
  tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), show);
  tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), show);
}
