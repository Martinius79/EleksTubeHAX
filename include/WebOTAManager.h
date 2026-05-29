#ifndef WEB_OTA_MANAGER_H
#define WEB_OTA_MANAGER_H

#include "GLOBAL_DEFINES.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "Clock.h"
#include "TFTs.h"
#include "Backlights.h"
#include "StoredConfig.h"

class WebOTAManager
{
public:
  void begin(Clock *uclock, TFTs *tfts, Backlights *backlights, StoredConfig *config, const char *deviceName);
  void handle();

private:
  Clock *_uclock = nullptr;
  TFTs *_tfts = nullptr;
  Backlights *_backlights = nullptr;
  StoredConfig *_config = nullptr;
  const char *_deviceName = nullptr;

  WebServer _server{80};
  bool _started = false;

  void setupRoutes();
  String buildMainPage();
  String buildStatusJson();
  void handleApiCommand();
};

#endif // WEB_OTA_MANAGER_H
