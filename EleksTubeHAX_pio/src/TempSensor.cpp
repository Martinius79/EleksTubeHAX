
#include "GLOBAL_DEFINES.h"
#include "TempSensor.h"

#ifdef ONE_WIRE_BUS_PIN

#include <OneWire.h>
#include <DallasTemperature.h>

float fTemperature = -127;
char sTemperatureTxt[10];
bool bTemperatureUpdated = false;
uint32_t lastTimeRead = (uint32_t)(TEMPERATURE_READ_EVERY_SEC * -1000);

// Setup a oneWire instance to communicate with OneWire devices (DS18B20)
OneWire oneWire(ONE_WIRE_BUS_PIN);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

void GetTemperatureDS18B20()
{
  Serial.print("Reading temperature... ");
  sensors.requestTemperatures();             // Request temperatures from all sensors on the bus
  fTemperature = sensors.getTempCByIndex(0); // Get temperature from the first sensor

  // Safely format the temperature into sTemperatureTxt
  snprintf(sTemperatureTxt, sizeof(sTemperatureTxt), "%.2f", fTemperature);

  bTemperatureUpdated = true;

  // Serial.print(fTemperature); // print float directly, for testing purposes only
  // Serial.print("  /  ");
  Serial.print(sTemperatureTxt);
  Serial.println(" C");
}

void PeriodicReadTemperature()
{
  if ((millis() - lastTimeRead) > (TEMPERATURE_READ_EVERY_SEC * 1000))
  {
    GetTemperatureDS18B20();
    lastTimeRead = millis();
  }
}
#endif // ONE_WIRE_BUS_PIN
