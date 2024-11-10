#ifndef TEMPSENSOR_H_
#define TEMPSENSOR_H_

#ifdef ONE_WIRE_BUS_PIN

extern float fTemperature;
extern char sTemperatureTxt[10];
extern bool bTemperatureUpdated;

void PeriodicReadTemperature(void);

#endif // ONE_WIRE_BUS_PIN

#endif // TEMPSENSOR_H_
