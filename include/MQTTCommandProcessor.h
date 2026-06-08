#ifndef MQTT_COMMAND_PROCESSOR_H
#define MQTT_COMMAND_PROCESSOR_H

#include "GLOBAL_DEFINES.h"

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)

#include "MQTT_client_ips.h"
#include "Backlights.h"
#include "TFTs.h"
#include "Clock.h"
#include "Menu.h"
#include "StoredConfig.h"

class MQTTCommandProcessor
{
public:
  void begin(TFTs *tfts, Backlights *backlights, Clock *uclock, StoredConfig *config, Menu *menu);
  void loop();
  void loopInFreeTime();

private:
  TFTs *_tfts = nullptr;
  Backlights *_backlights = nullptr;
  Clock *_uclock = nullptr;
  StoredConfig *_config = nullptr;
  Menu *_menu = nullptr;

  uint32_t _lastCommandExecuted = (uint32_t)-1;

  void processCommands();
  void updateStatus();
  void handleMainPower();
  void handleBackPower();
  void handleState();
  void handleMainBrightness();
  void handleBackBrightness();
  void handlePattern();
  void handleBackPattern();
  void handleBackColorPhase();
  void handleGraphic();
  void handleMainGraphic();
  void handleUseTwelveHours();
  void handleBlankZeroHours();
  void handlePulseBpm();
  void handleBreathBpm();
  void handleRainbowSec();
};

#else

class MQTTCommandProcessor
{
public:
  void begin(void *, void *, void *, void *, void *) {}
  void loop() {}
  void loopInFreeTime() {}
};

#endif

#endif
