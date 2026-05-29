#include "GestureHandler.h"

#ifdef HARDWARE_NOVELLIFE_CLOCK

GestureHandler *GestureHandler::instance = nullptr;

void GestureHandler::begin()
{
  instance = this;

  // Set interrupt pin on ESP32 as input
  pinMode(GESTURE_SENSOR_INPUT_PIN, INPUT);

  // Initialize interrupt service routine for APDS-9960 sensor
  attachInterrupt(digitalPinToInterrupt(GESTURE_SENSOR_INPUT_PIN), GestureHandler::interruptRoutine, FALLING);

  // Initialize gesture sensor APDS-9960 (configure I2C and initial values)
  if (apds.init())
  {
    Serial.println(F("APDS-9960 initialization complete"));

    // Set Gain to 1x, because the cheap chinese fake APDS sensor can't handle more
    // (also remember to extend ID check in SparkFun library to 0x3B!)
    apds.setGestureGain(GGAIN_1X);

    // Start running the APDS-9960 gesture sensor engine
    if (apds.enableGestureSensor(true))
    {
      Serial.println(F("Gesture sensor is now running"));
    }
    else
    {
      Serial.println(F("Something went wrong during gesture sensor enabling in the APDS-9960 library!"));
    }
  }
  else
  {
    Serial.println(F("Something went wrong during APDS-9960 init!"));
  }
}

void IRAM_ATTR GestureHandler::interruptRoutine()
{
  if (instance)
  {
    instance->isr_flag = 1;
  }
}

void GestureHandler::handle(Buttons &buttons)
{
  if (isr_flag == 1)
  {
    detachInterrupt(digitalPinToInterrupt(GESTURE_SENSOR_INPUT_PIN));
    handleGesture(buttons);
    isr_flag = 0;
    attachInterrupt(digitalPinToInterrupt(GESTURE_SENSOR_INPUT_PIN), GestureHandler::interruptRoutine, FALLING);
  }
}

void GestureHandler::handleGesture(Buttons &buttons)
{
  if (apds.isGestureAvailable())
  {
    switch (apds.readGesture())
    {
    case DIR_UP:
      buttons.left.setDownEdgeState();
      Serial.println("Gesture detected! LEFT");
      break;
    case DIR_DOWN:
      buttons.right.setDownEdgeState();
      Serial.println("Gesture detected! RIGHT");
      break;
    case DIR_LEFT:
      buttons.power.setDownEdgeState();
      Serial.println("Gesture detected! DOWN");
      break;
    case DIR_RIGHT:
      buttons.mode.setDownEdgeState();
      Serial.println("Gesture detected! UP");
      break;
    case DIR_NEAR:
      buttons.mode.setDownEdgeState();
      Serial.println("Gesture detected! NEAR");
      break;
    case DIR_FAR:
      buttons.power.setDownEdgeState();
      Serial.println("Gesture detected! FAR");
      break;
    default:
      Serial.println("Movement detected but NO gesture detected!");
    }
  }
}

#endif // HARDWARE_NOVELLIFE_CLOCK
