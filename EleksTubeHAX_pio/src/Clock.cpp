#include "Clock.h"
#include "WiFi_WPS.h"

#if defined(HARDWARE_SI_HAI_CLOCK) || defined(HARDWARE_IPSTUBE_CLOCK) // for Clocks with DS1302 chip #SI HAI or IPSTUBE XXXXXXXXXXXXXXXXXX
// If it is a SI HAI Clock, use differnt RTC chip drivers
#include <ThreeWire.h>
#include <RtcDS1302.h>
ThreeWire myWire(DS1302_IO, DS1302_SCLK, DS1302_CE); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);
void RtcBegin()
{
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: RtcBegin() for DS1302 RTC entered.");
#endif
  Rtc.Begin();
  // check if chip is connected and alive
  /*  TCS default value is 0x00 instead of 0x5C, but RTC seems to be working. Let's skip this check.
      Serial.print("Checking DS1302 RTC... ");
      uint8_t TCS = Rtc.GetTrickleChargeSettings();  // 01011100  Initial power-on state
      Serial.print("TCS = ");
      Serial.println(TCS);
      if (TCS != 0x5C) {
        Serial.println("Error communicating with DS1302 !");
      }
  */
  if (!Rtc.IsDateTimeValid())
  {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    Serial.println("RTC lost confidence in the DateTime!");
  }
  if (Rtc.GetIsWriteProtected())
  {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }
}

uint32_t RtcGet()
{
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: RtcGet() for DS1302 RTC entered.");
  Serial.println("DEBUG: Calling Rtc.GetDateTime()...");
#endif
  RtcDateTime temptime;
  temptime = Rtc.GetDateTime();
  uint32_t returnvalue = temptime.Unix32Time();
#ifdef DEBUG_OUTPUT
  Serial.print("DEBUG: RtcGet() DS1302 returned: ");
  Serial.println(returnvalue);
#endif
  return returnvalue;
}

void RtcSet(uint32_t tt)
{
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: RtcSet() for DS1302 RTC entered.");
  Serial.print("DEBUG: Setting DS1302 RTC to: ");
  Serial.println(tt);
#endif
  RtcDateTime temptime;
  temptime.InitWithUnix32Time(tt);
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: DS1302 RTC time set.");
#endif
  Rtc.SetDateTime(temptime);
}
#elif defined(HARDWARE_NovelLife_SE_CLOCK) // for NovelLife_SE clone with R8025T RTC chip
#include <RTC_RX8025T.h>                   // This header will now use Wire1 for I²C operations.

RX8025T RTC;

void RtcBegin()
{
#ifdef DEBUG_OUTPUT
  Serial.println("");
  Serial.println("DEBUG: RtcBegin() for RX8025T RTC entered.");
#endif
  RTC.init((uint32_t)RTC_SDA_PIN, (uint32_t)RTC_SCL_PIN); // setup second I2C for the RX8025T RTC chip
  Serial.println("RTC RX8025T initialized!");
  delay(100);
  return;
}

void RtcSet(uint32_t tt)
{
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: RtcSet() for RX8025T RTC entered.");
  Serial.print("DEBUG: Setting RX8025T RTC to: ");
  Serial.println(tt);
#endif

  int ret = RTC.set(tt);
  if (ret != 0)
  {
    Serial.print("Error setting RX8025T RTC: ");
    Serial.println(ret);
  }
  else
  {
#ifdef DEBUG_OUTPUT
    Serial.println("RX8025T RTC time set successfully!");
#endif
  }
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: RX8025T RTC time set.");
#endif
}

uint32_t RtcGet()
{
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: RtcGet() for RX8025T RTC entered.");
#endif
  uint32_t returnvalue = RTC.get(); // Get the RTC time
#ifdef DEBUG_OUTPUT
  Serial.print("DEBUG: RtcGet() RX8025T returned: ");
  Serial.println(returnvalue);
#endif
  return returnvalue;
}

// #include <Wire.h>
//  #include <RTC_RX8025T.h>

// void scanI2CBus() {
//   byte error, address;
//   int nDevices = 0;
//   Serial.println("Scanning I2C bus...");

//   for(address = 1; address < 127; address++ ) {
//     Wire.beginTransmission(address);
//     error = Wire.endTransmission();

//     if (error == 0) {
//       Serial.print("I2C device FOUND at 0x");
//       if (address < 16)
//         Serial.print("0");
//       Serial.println(address, HEX);
//       nDevices++;
//     } else if (error == 4) {
//       Serial.print("Unknown error at address 0x");
//       if(address < 16)
//         Serial.print("0");
//       Serial.println(address, HEX);
//     }
//   }

//   if (nDevices == 0)
//     Serial.println("No I2C devices found.");
//   else
//     Serial.println("done.");
// }

// void RtcBegin()
// {
//   // scanI2CBus(); // Scan the I2C bus for devices
//   //RX8025T initialization
//   RTC_RX8025T.init();
//   Serial.println("RTC init!");
//   delay(100);
// };

// uint32_t RtcGet()
// {
// #ifdef DEBUG_OUTPUT
//   Serial.println("RtcGet() for RX8025T RTC chip entered.");
// #endif
//   uint32_t returnvalue = RTC_RX8025T.get(); // Get the RTC time
//   if (returnvalue == 0)
//   {
//     Serial.println("Error getting RTC time!");
//   }
//   else
//   {
// #ifdef DEBUG_OUTPUT
//   Serial.print("RtcGet() = ");
//   Serial.println(returnvalue);
// #endif
//   }
//   return returnvalue;
// };

// void RtcSet(uint32_t tt)
// {
// #ifdef DEBUG_OUTPUT
//   Serial.println("RtcSet() for RX8025T RTC chip entered.");
// #endif
//   Serial.print("Trying to set time now to: ");
//   Serial.println(tt);
//   int ret = RTC_RX8025T.set(tt); // Set the RTC time
//   if (ret != 0)
//   {
//     Serial.print("Error setting RTC time: ");
//     Serial.println(ret);
//   }else{
//     Serial.println("RTC time set successfully!");
//     Serial.print("RTCset() return value: ");
//     Serial.println(ret);
//   }
// #ifdef DEBUG_OUTPUT
//   Serial.print("RtcSet() = ");
//   Serial.println(RTC_RX8025T.get()); // Get the RTC time again, because it may have changed in the meantime
// #endif
//   delay(100);
// }

#else // for Elekstube and other clocks with DS3231 RTC chip
// for if defined(HARDWARE_SI_HAI_CLOCK) || defined(HARDWARE_IPSTUBE_CLOCK)
// Alternatively for the DS1307 RTC chip
// #include <DS1307RTC.h>
#include <RTClib.h>

RTC_DS3231 RTC; // Funktioniert auch mit DS1307 oder PCF8523

void RtcBegin()
{
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: RtcBegin() for DS3231 RTC entered.");
#endif
  if (!RTC.begin())
  {
    Serial.println("NO supported RTC found!");
  }
  else
  {
    Serial.println("RTC found!");
  }
  Wire.begin(); // Standard: GPIO21 (SDA) & GPIO22 (SCL) for ESP32

  Serial.println("Scanning I2C devices...");
  for (uint8_t address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0)
    {
      Serial.print("I2C device FOUND at 0x");
      Serial.println(address, HEX);
    }
    else
    {
      Serial.print("I2C device NOT at 0x");
      Serial.println(address, HEX);
    }
  }
}

uint32_t RtcGet()
{
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: RtcGet() for DS3231 RTC entered.");
#endif
  DateTime now = RTC.now(); // convert to unix time
  uint32_t returnvalue = now.unixtime();
#ifdef DEBUG_OUTPUT
  Serial.print("DEBUG: DS3231 RTC now.unixtime() = ");
  Serial.println(returnvalue);
#endif
  return returnvalue;
}

void RtcSet(uint32_t tt)
{
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: RtcSet() for DS3231 RTC entered.");
  Serial.print("DEBUG: Attempting to set DS3231 RTC to: ");
  Serial.println(tt);
#endif

  DateTime timetoset(tt); // convert to unix time
  RTC.adjust(timetoset);  // set the RTC time
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG: DS3231 RTC time updated.");
#endif
}
#endif // end of RTC chip selection, else of (if defined(HARDWARE_SI_HAI_CLOCK) || defined(HARDWARE_IPSTUBE_CLOCK))

void Clock::begin(StoredConfig::Config::Clock *config_)
{
  config = config_;

  if (config->is_valid != StoredConfig::valid)
  {
    // Config is invalid, probably a new device never had its config written.
    // Load some reasonable defaults.
    Serial.println("Loaded Clock config is invalid, using default.  This is normal on first boot.");
    setTwelveHour(false);
    setBlankHoursZero(false);
    setTimeZoneOffset(1 * 3600); // CET
    setActiveGraphicIdx(1);
    config->is_valid = StoredConfig::valid;
  }

  RtcBegin();
  ntpTimeClient.begin();
  ntpTimeClient.update();
  Serial.print("NTP time = ");
  Serial.println(ntpTimeClient.getFormattedTime());
  setSyncProvider(&Clock::syncProvider);
}

void Clock::loop()
{
  if (timeStatus() == timeNotSet)
  {
    time_valid = false;
  }
  else
  {
    loop_time = now();
    local_time = loop_time + config->time_zone_offset;
    time_valid = true;
  }
}

// Static methods used for sync provider to TimeLib library.
time_t Clock::syncProvider()
{
#ifdef DEBUG_OUTPUT
  Serial.println("Clock:syncProvider() entered.");
#endif
  time_t rtc_now;
  rtc_now = RtcGet(); // Get the RTC time

  if (millis() - millis_last_ntp > refresh_ntp_every_ms || millis_last_ntp == 0) // Get NTP time only every 10 minutes or if not yet done
  {                                                                              // It's time to get a new NTP sync
    if (WifiState == connected)
    { // We have WiFi, so try to get NTP time.
      Serial.print("Try to get the actual time from NTP server...");
      if (ntpTimeClient.update())
      {
        Serial.print(".");
        time_t ntp_now = ntpTimeClient.getEpochTime();
        Serial.println("Done!");
        Serial.print("NTP time = ");
        Serial.println(ntpTimeClient.getFormattedTime());
        rtc_now = RtcGet(); // Get the RTC time again, because it may have changed in the meantime
        // Sync the RTC to NTP if needed.
        Serial.print("NTP  :");
        Serial.println(ntp_now);
        Serial.print("RTC  :");
        Serial.println(rtc_now);
        Serial.print("Diff: ");
        Serial.println(ntp_now - rtc_now);
        if ((ntp_now != rtc_now) && (ntp_now > 1743364444)) // check if we have a difference and a valid NTP time
        {                                                   // NTP time is valid and different from RTC time
          Serial.println("RTC time is not valid, updating RTC.");
          RtcSet(ntp_now);
          Serial.println("RTC is now set to NTP time.");
          rtc_now = RtcGet(); // Check if RTC time is set correctly
          Serial.print("RTC time = ");
          Serial.println(rtc_now);
        }
        else if ((ntp_now != rtc_now) && (ntp_now < 1743364444))
        { // NTP can't be valid!
          Serial.println("Time returned from NTP is not valid! Using RTC time!");
          rtc_now = RtcGet(); // Get the RTC time again, because it may have changed in the meantime
          return rtc_now;
        }
        millis_last_ntp = millis(); // store the last time we tried to get NTP time

        Serial.println("Using NTP time!");
        return ntp_now;
      }
      else
      {                     // NTP return not valid
        rtc_now = RtcGet(); // Get the RTC time again, because it may have changed in the meantime
        Serial.println("Invalid NTP response, using RTC time.");
        return rtc_now;
      }
    } // no WiFi!
    Serial.println("No WiFi, using RTC time.");
    return rtc_now;
  }
  Serial.println("Using RTC time.");
  return rtc_now;
}

uint8_t Clock::getHoursTens()
{
  uint8_t hour_tens = getHour() / 10;

  if (config->blank_hours_zero && hour_tens == 0)
  {
    return TFTs::blanked;
  }
  else
  {
    return hour_tens;
  }
}

uint32_t Clock::millis_last_ntp = 0;
WiFiUDP Clock::ntpUDP;
NTPClient Clock::ntpTimeClient(ntpUDP);
