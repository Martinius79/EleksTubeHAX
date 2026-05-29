#ifndef GESTURE_HANDLER_H
#define GESTURE_HANDLER_H

#include "GLOBAL_DEFINES.h"
#include "Buttons.h"

#ifdef HARDWARE_NOVELLIFE_CLOCK

#include <Wire.h>
#include <SparkFun_APDS9960.h>

class GestureHandler
{
public:
  void begin();
  void handle(Buttons &buttons);

private:
  SparkFun_APDS9960 apds;
  volatile int isr_flag = 0;

  void handleGesture(Buttons &buttons);
  static void IRAM_ATTR interruptRoutine();
  static GestureHandler *instance; // For ISR callback
};

#else // No gesture sensor

class GestureHandler
{
public:
  void begin() {}
  void handle(Buttons &buttons) { (void)buttons; }
};

#endif // HARDWARE_NOVELLIFE_CLOCK

#endif // GESTURE_HANDLER_H
