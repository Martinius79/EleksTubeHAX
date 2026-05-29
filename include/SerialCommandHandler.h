#ifndef SERIAL_COMMAND_HANDLER_H
#define SERIAL_COMMAND_HANDLER_H

#include "GLOBAL_DEFINES.h"
#include "Clock.h"
#include "TFTs.h"
#include "Backlights.h"
#include "StoredConfig.h"

class SerialCommandHandler
{
public:
  void begin(Clock *uclock, TFTs *tfts, Backlights *backlights, StoredConfig *config);
  void handle();

private:
  Clock *_uclock = nullptr;
  TFTs *_tfts = nullptr;
  Backlights *_backlights = nullptr;
  StoredConfig *_config = nullptr;

  String _inputBuffer;

  void processCommand(const String &cmd);
  void printHelp();
  void printStatus();
  void printTime();
  void printNetwork();
  void setBrightness(const String &args);
  void setBacklightBrightness(const String &args);
  void setPattern(const String &args);
  void setGraphic(const String &args);
  void setTimeFormat(const String &args);
  void setPower(const String &args);
  void setBacklightPower(const String &args);
  void saveConfig();
  void reboot();
};

#endif // SERIAL_COMMAND_HANDLER_H
