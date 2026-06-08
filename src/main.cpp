/*
 * Author: Aljaz Ogrin
 * Project: Alternative firmware for EleksTube IPS clock
 * Original location: https://github.com/aly-fly/EleksTubeHAX
 * Hardware: ESP32
 * Based on: https://github.com/SmittyHalibut/EleksTubeHAX
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

#include "GestureHandler.h"
#include "MQTTCommandProcessor.h"
#include "MenuRenderer.h"
#include "GeolocationManager.h"
#include "DimmingManager.h"
#include "PowerManager.h"
#include "DisplayHelpers.h"

char UniqueDeviceName[32];

Backlights backlights;
Buttons buttons;
TFTs tfts;
Clock uclock;
Menu menu;
StoredConfig stored_config;

GestureHandler gestureHandler;
MQTTCommandProcessor mqttProcessor;
MenuRenderer menuRenderer;
GeolocationManager geolocation;
DimmingManager dimming;
PowerManager powerManager;

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println("\nSystem starting...\n");
  Serial.println("EleksTubeHAX https://github.com/aly-fly/EleksTubeHAX");
  Serial.printf("Firmware version: v%s.\n", FIRMWARE_VERSION);

#ifdef MQTT_CLIENT_ID_FOR_SMARTNEST
  snprintf(UniqueDeviceName, sizeof(UniqueDeviceName), "%s", MQTT_CLIENT_ID_FOR_SMARTNEST);
#else
  {
    uint64_t rawmac = ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL;
    uint8_t mac_bytes[6];
    for (int i = 0; i < 6; i++)
    {
      mac_bytes[i] = (rawmac >> (8 * i)) & 0xFF;
    }
    snprintf(UniqueDeviceName, sizeof(UniqueDeviceName), "%s-%02X%02X%02X", DEVICE_NAME,
             mac_bytes[3], mac_bytes[4], mac_bytes[5]);
  }
#endif
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

  Serial.print("Init NVS flash partition usage...");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    Serial.println("");
    Serial.println("No free pages in or newer version of NVS partition found. Erasing NVS flash partition...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  Serial.println("Done.");

  stored_config.begin();
  stored_config.load();

  backlights.begin(&stored_config.config.backlights);
#ifndef NO_BUTTONS
  buttons.begin();
#endif
  menu.begin();

  tfts.begin();
  tfts.fillScreen(TFT_BLACK);

  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
#ifdef DISPLAY_SMALL
  tfts.setCursor(0, 0, 1);
#else
  tfts.setCursor(0, 0, 2);
#endif
  tfts.println("Starting Setup...");

#ifdef HARDWARE_NOVELLIFE_CLOCK
  tfts.setTextColor(TFT_ORANGE, TFT_BLACK);
  tfts.print("Gest start...");
  Serial.print("Gesture Sensor start...");
  gestureHandler.begin();
  tfts.println("Done!");
  Serial.println("Done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
#endif

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

  tfts.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tfts.print("Clock start...");
  Serial.println("\nClock start-up...");
  uclock.begin(&stored_config.config.uclock);
  tfts.println("Done!");
  Serial.println("\nClock start-up done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
  tfts.setTextColor(TFT_YELLOW, TFT_BLACK);
  tfts.print("MQTT start...");
  Serial.println("\nMQTT start...");
  MQTTStart(false);
  mqttProcessor.begin(&tfts, &backlights, &uclock, &stored_config, &menu);
  tfts.println("Done!");
  Serial.println("MQTT start Done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
#endif

#ifdef GEOLOCATION_ENABLED
  tfts.setTextColor(TFT_CYAN, TFT_BLACK);
  tfts.println("GeoLoc query...");
  geolocation.begin(&uclock, &stored_config);
  if (geolocation.queryTimezoneOffset())
  {
    int32_t tzOffsetH = uclock.getTimeZoneOffset() / 3600;
    tfts.print("TZ: ");
    Serial.print("TZ: ");
    tfts.println(tzOffsetH);
    Serial.println(tzOffsetH);
    tfts.println("Done!");
    Serial.println("Done!");
  }
  else
  {
    tfts.setTextColor(TFT_RED, TFT_BLACK);
    tfts.println("GeoLoc FAILED");
    Serial.println("GeoLoc failed!");
    tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  }
#endif

  if (uclock.getActiveGraphicIdx() > tfts.NumberOfClockFaces)
  {
    uclock.setActiveGraphicIdx(tfts.NumberOfClockFaces);
    Serial.println("Last selected index of clock face is larger than currently available number of image sets.");
  }
  if (uclock.getActiveGraphicIdx() < 1)
  {
    uclock.setActiveGraphicIdx(1);
    Serial.println("Last selected index of clock face is less than 1.");
  }
  tfts.current_graphic = uclock.getActiveGraphicIdx();

  menuRenderer.begin(&tfts, &uclock, &backlights, &stored_config);
  powerManager.begin(&tfts, &backlights, &uclock);

#ifdef DIMMING
  dimming.begin(&uclock, &tfts, &backlights, &stored_config);
#endif

  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.println("Done with Setup!");
  Serial.println("\nDone with Setup!");

  for (uint8_t ndx = 0; ndx < 10; ndx++)
  {
    tfts.print(">");
    delay(200);
  }

  tfts.fillScreen(TFT_BLACK);
  uclock.loop();
  updateClockDisplay(tfts, uclock, TFTs::force);
  Serial.println("Starting main loop...");
}

void loop()
{
  uint32_t millis_at_top = millis();

  WifiReconnect();

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
  mqttProcessor.loop();
#endif

#ifndef NO_BUTTONS
  buttons.loop();
#endif

#ifdef HARDWARE_NOVELLIFE_CLOCK
  gestureHandler.handle(buttons);
#endif

#ifndef ONE_BUTTON_ONLY_MENU
  if (buttons.power.isDownEdge() && (menu.getState() == Menu::idle))
  {
    powerManager.handlePowerButton();
  }
#endif

  menu.loop(buttons);

#ifdef CAPACITIVE_TOUCH_BUTTONS
  if (menu.isPowerToggle())
  {
    powerManager.handlePowerToggle();
  }
#endif

  backlights.loop();
  uclock.loop();

#ifdef DIMMING
  dimming.check();
#endif

  updateClockDisplay(tfts, uclock);

  menuRenderer.loop(menu);

#ifdef GEOLOCATION_ENABLED
  geolocation.checkUpdateNeeded();
#endif

  uint32_t time_in_loop = millis() - millis_at_top;
  if (time_in_loop < 20)
  {
    tfts.LoadNextImage();

    time_in_loop = millis() - millis_at_top;
    if (time_in_loop < 20)
    {
#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
      mqttProcessor.loopInFreeTime();
#endif

#ifdef GEOLOCATION_ENABLED
      geolocation.processUpdate();
#endif

      time_in_loop = millis() - millis_at_top;
      if (time_in_loop < 20)
      {
        delay(20 - time_in_loop);
      }
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
