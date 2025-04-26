#include "ChipSelect.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#ifdef HARDWARE_IPSTUBE_CLOCK
// Define the pins for each LCD's enable wire
// The order is from left to right, so the first pin is for the seconds ones, the last for the hours tens
// LCD2 is the leftmost one         - seconds one - pin 21 as GPIO15
// LCD3 is the second from the left - seconds ten - pin 22 as GPIO2
// LCD4 is the third from the left  - minutes one - pin 23 as GPIO27
// LCD5 is the fourth from the left - minutes ten - pin 17 as GPIO14
// LCD6 is the fifth from the left  - hours one   - pin 18 as GPIO12
// LCD7 is the rightmost one        - hours ten   - pin 20 as GPIO13
const uint32_t lcdEnablePins[NUM_DIGITS] = {GPIO_NUM_15, GPIO_NUM_2, GPIO_NUM_27, GPIO_NUM_14, GPIO_NUM_12, GPIO_NUM_13};
const uint32_t numLCDs = NUM_DIGITS;

const uint32_t allCSPinMask = (1 << GPIO_NUM_15) | (1 << GPIO_NUM_2) | (1 << GPIO_NUM_27) |
                              (1 << GPIO_NUM_14) | (1 << GPIO_NUM_12) | (1 << GPIO_NUM_13);

uint32_t activOrLOWCSPinMask = 0; // Mask for the currently active CS pin
uint32_t inactiveOrHIGHCSPinMask = 0; // Mask for the currently inactive CS pin (all pins should be set inactive at the beginning)

#endif

void ChipSelect::begin()
{
  #ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::begin - Initializing ChipSelect...");
  #endif
#ifndef HARDWARE_IPSTUBE_CLOCK
  pinMode(CSSR_LATCH_PIN, OUTPUT);
  pinMode(CSSR_DATA_PIN, OUTPUT);
  pinMode(CSSR_CLOCK_PIN, OUTPUT);

  digitalWrite(CSSR_DATA_PIN, LOW);
  digitalWrite(CSSR_CLOCK_PIN, LOW);
  digitalWrite(CSSR_LATCH_PIN, LOW);
  update();
#else
  // Initialize all six different pins for the CS of each LCD as OUTPUT and set it to HIGH (disabled)
  // uint32_t pinMask = 0;
  // for (int i = 0; i < numLCDs; ++i)
  // {
  //   pinMask |= (1 << lcdEnablePins[i]);
  // }

#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::begin - allCSPinMask: ");
  Serial.println(allCSPinMask, BIN);
  Serial.print("ChipSelect::begin - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::begin - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);

  Serial.println("ChipSelect::begin - Disabeling LEDC on Display pins.");
#endif
  ledcDetachPin(GPIO_NUM_15);
  ledcDetachPin(GPIO_NUM_2);
  ledcDetachPin(GPIO_NUM_27);
  ledcDetachPin(GPIO_NUM_14);
  ledcDetachPin(GPIO_NUM_12);
  ledcDetachPin(GPIO_NUM_13);
  // Set all pins as OUTPUT
  GPIO.enable_w1ts = allCSPinMask;
  delayMicroseconds(1);

  // Set all pins to HIGH (disabled)
  GPIO.out_w1ts = allCSPinMask;
  delayMicroseconds(1);

  inactiveOrHIGHCSPinMask = allCSPinMask; // Set the active CS pin mask to all pins (all disabled)
  activOrLOWCSPinMask = 0; // Set the inactive CS pin mask to 0 (no pins enabled)


#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::begin - Initialized ALL CS pins as OUTPUT and set to HIGH (disabled)");
  Serial.print("ChipSelect::begin - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::begin - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
  Serial.print("ChipSelect::begin - Finished!");
#endif
#endif
}

void ChipSelect::clear(bool update_)
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  setDigitMap(all_off, update_);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::clear - Setting all CS pins to HIGH (disabled) by calling disableAllCSPins()");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
  disableAllCSPins();
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::clear - All CS pins disabled");
  Serial.println("ChipSelect::clear - Finshed!");
#endif
#endif
}

void ChipSelect::setAll(bool update_)
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  setDigitMap(all_on, update_);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::setAll - Setting all CS pins to LOW (enabled) by calling enableAllCSPins()");
#endif
  enableAllCSPins();
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::setAll - All CS pins enabled");
  Serial.println("ChipSelect::setAll - Finshed!");
#endif
#endif
}

void ChipSelect::setDigit(uint8_t digit, bool update_)
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  // Set the bit for the given digit in the digits_map
  setDigitMap(1 << digit, update_);
  if (update_)
    update();
#else
  // Set the actual currentLCD value for the given digit and activate the corresponding LCD
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::setDigit - digit to activate: ");
  Serial.println(digit);
  Serial.print("Selected digit from last run or elsewhere ('old' currentLCD): ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::setDigit - Current LCD pin: ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::setDigit - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.print("ChipSelect::setDigit - activeOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::setDigit - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
  
  uint32_t disableMask = (1 << lcdEnablePins[currentLCD]);
  uint32_t enableMask = (1 << lcdEnablePins[digit]);

#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::setDigit - disableMask: ");
  Serial.println(disableMask, BIN);
  Serial.print("ChipSelect::setDigit - enableMask: ");
  Serial.println(enableMask, BIN);
#endif

  GPIO.out_w1ts = disableMask; // Set the current LCD pin HIGH (disable)
  delayMicroseconds(1);

  GPIO.out_w1tc = enableMask; // Set the new LCD pin LOW (enable)
  delayMicroseconds(1);

  currentLCD = digit;

  //subtract the deacivated pin from the activeorHIGHCSpinmask
  activOrLOWCSPinMask &= ~disableMask; // Set the current LCD pin HIGH (disable)  
  //add the activated pin to the activeorHIGHCSpinmask
  activOrLOWCSPinMask |= enableMask; // Set the new LCD pin LOW (enable)

  //add the deactivated pin to the inactiveorHIGHCSpinmask
  inactiveOrHIGHCSPinMask |= disableMask; // Set the current LCD pin HIGH (disable)
  
  //subtract the deacivated pin from the inactiveorHIGHCSpinmask  
  inactiveOrHIGHCSPinMask &= ~enableMask; // Set the new LCD pin LOW (enable)  
  
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::setDigit - Activated digit ('new' currentLCD): ");
  Serial.println(digit);
  Serial.print("ChipSelect::setDigit - CurrentLCD: ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::setDigit - Current LCD pin: ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::setDigit - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.print("ChipSelect::setDigit - activeOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::setDigit - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
  Serial.println("ChipSelect::setDigit - Finished!");
#endif
  // NO UPDATE, cause update enables and disables the pin, but needs to be "enabled", while eTFT_SPI is writing into it.
#endif
}

void ChipSelect::update()
{
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::update - Updating ChipSelect...");
#endif
#ifndef HARDWARE_IPSTUBE_CLOCK
  // Documented in README.md.  Q7 and Q6 are unused. Q5 is Seconds Ones, Q0 is Hours Tens.
  // Q7 is the first bit written, Q0 is the last.  So we push two dummy bits, then start with
  // Seconds Ones and end with Hours Tens.
  // CS is Active Low, but digits_map is 1 for enable, 0 for disable.  So we bit-wise NOT first.

  uint8_t to_shift = (~digits_map) << 2;

  digitalWrite(CSSR_LATCH_PIN, LOW);
  shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, LSBFIRST, to_shift);
  digitalWrite(CSSR_LATCH_PIN, HIGH);
#else
  // this is just, to follow the "update" logic of the other hardware!
  // for IPSTUBE clocks, the CS pin is already pulled to LOW by the "setDigit" function and stays there, till another "setDigit" is called.
  // so all writing done by the eTFT_SPI lib functions in the time, the pin is low, will write out directly to the LCD.
  //"Update" never will work, because, if pin was HIGH, no writing was done.
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::update - Current LCD: ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::update - Current LCD pin: ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::update - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.print("ChipSelect::update - activCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::update - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif

  //check if the current LCD is already enabled
  if ((activOrLOWCSPinMask & (1 << lcdEnablePins[currentLCD])) == 0)
  {
    GPIO.out_w1tc = (1 << lcdEnablePins[currentLCD]); // Set the current LCD pin LOW (enable)
    delayMicroseconds(1);    
    
    //add the activated pin to the activeorLOWCSpinmask
    activOrLOWCSPinMask |= (1 << lcdEnablePins[currentLCD]); // Set the current LCD pin LOW (enable)
    //subtract the acivated pin from the inactiveorHIGHCSpinmask
    inactiveOrHIGHCSPinMask &= ~(1 << lcdEnablePins[currentLCD]); // Set the current LCD pin LOW (enable)    
  }
#ifdef DEBUG_OUTPUT_CHIPSELECT
  else
  {

    Serial.print("ChipSelect::update - Current LCD pin is already LOW (enabled): ");
    Serial.println(lcdEnablePins[currentLCD]);
    Serial.print("ChipSelect::update - activCSPinMask: ");
    Serial.println(activOrLOWCSPinMask, BIN);
    Serial.print("ChipSelect::update - inactiveOrHIGHCSPinMask: ");
    Serial.println(inactiveOrHIGHCSPinMask, BIN);  
  }
  Serial.print("ChipSelect::update - Current LCD pin is LOW (enabled): ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::update - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::update - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
  Serial.print("ChipSelect::update - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.println("ChipSelect::update - Finished!");
#endif
#endif
}

bool ChipSelect::isSecondsOnes()
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  return ((digits_map & SECONDS_ONES_MAP) > 0);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::isSecondsOnes - Current LCD: ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::isSecondsOnes - Current LCD pin: ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::isSecondsOnes - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.print("ChipSelect::isSecondsOnes - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::isSecondsOnes - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
  Serial.print("ChipSelect::isSecondsOnes - SECONDS_ONES: ");
  Serial.println(SECONDS_ONES);
  Serial.print("ChipSelect::isSecondsOnes - SECONDS_ONES: ");
  Serial.println(SECONDS_ONES, BIN);
  Serial.print("ChipSelect::isSecondsOnes - SECONDS_ONES pin: ");
  Serial.println(lcdEnablePins[SECONDS_ONES]);
  Serial.print("ChipSelect::isSecondsOnes - SECONDS_ONES pin mask: ");
  Serial.println(1 << lcdEnablePins[SECONDS_ONES], BIN);

#endif
  // Check if the current LCD is the seconds ones LCD
  if (currentLCD == SECONDS_ONES)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isSecondsOnes - Current LCD is SECONDS_ONES!");    
#endif
    //return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isSecondsOnes - Current LCD is NOT SECONDS_ONES!");
#endif
    //return false;
  }
  //Second way to check if the current LCD is the seconds ones LCD
  // Check if the current LCD is the seconds ones LCD
  if (activOrLOWCSPinMask == (1 << lcdEnablePins[SECONDS_ONES]))
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isSecondsOnes - Current LCD is SECONDS_ONES - 2nd way!");
    Serial.print("ChipSelect::isSecondsOnes - Finished with true!");
#endif
    return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::isSecondsOnes - Current LCD is NOT SECONDS_ONES - 2nd way!");
  Serial.println("ChipSelect::isSecondsOnes - Finished with false!");
#endif

    return false;
  }
  //return true;
#endif
}

bool ChipSelect::isSecondsTens()
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  return ((digits_map & SECONDS_TENS_MAP) > 0);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::isSecondTens - Current LCD: ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::isSecondTens - Current LCD pin: ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::isSecondTens - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.print("ChipSelect::isSecondTens - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::isSecondTens - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
  // Check if the current LCD is the seconds ones LCD
  if (currentLCD == SECONDS_TENS)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isSecondTens - Current LCD is SECONDS_TENS!");    
#endif
    //return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isSecondTens - Current LCD is NOT SECONDS_TENS!");
#endif
    //return false;
  }
  //Second way to check if the current LCD is the seconds ones LCD
  // Check if the current LCD is the seconds ones LCD
  if (activOrLOWCSPinMask == (1 << lcdEnablePins[SECONDS_TENS]))
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isSecondTens - Current LCD is SECONDS_TENS - 2nd way!");
    Serial.print("ChipSelect::isSecondTens - Finished with true!");
#endif
    return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::isSecondTens - Current LCD is NOT SECONDS_TENS - 2nd way!");
  Serial.println("ChipSelect::isSecondTens - Finished with false!");
#endif

    return false;
  }
  //return true;
#endif
}

bool ChipSelect::isMinutesOnes()
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  return ((digits_map & MINUTES_ONES_MAP) > 0);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::isMinutesOnes - Current LCD: ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::isMinutesOnes - Current LCD pin: ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::isMinutesOnes - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.print("ChipSelect::isMinutesOnes - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::isMinutesOnes - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
  // Check if the current LCD is the seconds ones LCD
  if (currentLCD == MINUTES_ONES)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isMinutesOnes - Current LCD is MINUTES_ONES!");    
#endif
    //return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isMinutesOnes - Current LCD is NOT MINUTES_ONES!");
#endif
    //return false;
  }
  //Second way to check if the current LCD is the seconds ones LCD
  // Check if the current LCD is the seconds ones LCD
  if (activOrLOWCSPinMask == (1 << lcdEnablePins[MINUTES_ONES]))
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isMinutesOnes - Current LCD is MINUTES_ONES - 2nd way!");
    Serial.print("ChipSelect::isMinutesOnes - Finished with true!");
#endif
    return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::isMinutesOnes - Current LCD is NOT MINUTES_ONES - 2nd way!");
  Serial.println("ChipSelect::isMinutesOnes - Finished with false!");
#endif

    return false;
  }
  //return true;
#endif
}

bool ChipSelect::isMinutesTens()
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  return ((digits_map & MINUTES_TENS_MAP) > 0);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::isMinutesTens - Current LCD: ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::isMinutesTens - Current LCD pin: ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::isMinutesTens - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.print("ChipSelect::isMinutesTens - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::isMinutesTens - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
  // Check if the current LCD is the seconds ones LCD
  if (currentLCD == MINUTES_TENS)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isMinutesTens - Current LCD is MINUTES_TENS!");    
#endif
    //return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isMinutesTens - Current LCD is NOT MINUTES_TENS!");
#endif
    //return false;
  }
  //Second way to check if the current LCD is the seconds ones LCD
  // Check if the current LCD is the seconds ones LCD
  if (activOrLOWCSPinMask == (1 << lcdEnablePins[MINUTES_TENS]))
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isMinutesTens - Current LCD is MINUTES_TENS - 2nd way!");
    Serial.print("ChipSelect::isMinutesTens - Finished with true!");
#endif
    return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::isMinutesTens - Current LCD is NOT MINUTES_TENS - 2nd way!");
  Serial.println("ChipSelect::isMinutesTens - Finished with false!");
#endif

    return false;
  }
  //return true;
#endif
}

bool ChipSelect::isHoursOnes()
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  return ((digits_map & HOURS_ONES_MAP) > 0);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::isHoursOnes - Current LCD: ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::isHoursOnes - Current LCD pin: ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::isHoursOnes - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.print("ChipSelect::isHoursOnes - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::isHoursOnes - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
  // Check if the current LCD is the seconds ones LCD
  if (currentLCD == HOURS_ONES)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isHoursOnes - Current LCD is HOURS_ONES!");    
#endif
    //return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isHoursOnes - Current LCD is NOT HOURS_ONES!");
#endif
    //return false;
  }
  //Second way to check if the current LCD is the seconds ones LCD
  // Check if the current LCD is the seconds ones LCD
  if (activOrLOWCSPinMask == (1 << lcdEnablePins[HOURS_ONES]))
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isHoursOnes - Current LCD is HOURS_ONES - 2nd way!");
    Serial.print("ChipSelect::isHoursOnes - Finished with true!");
#endif
    return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::isHoursOnes - Current LCD is NOT HOURS_ONES - 2nd way!");
  Serial.println("ChipSelect::isHoursOnes - Finished with false!");
#endif

    return false;
  }
  //return true;
#endif
}

bool ChipSelect::isHoursTens()
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  return ((digits_map & HOURS_TENS_MAP) > 0);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::isHoursTens - Current LCD: ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::isHoursTens - Current LCD pin: ");
  Serial.println(lcdEnablePins[currentLCD]);
  Serial.print("ChipSelect::isHoursTens - Current LCD pin mask: ");
  Serial.println(1 << lcdEnablePins[currentLCD], BIN);
  Serial.print("ChipSelect::isHoursTens - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::isHoursTens - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
  // Check if the current LCD is the seconds ones LCD
  if (currentLCD == HOURS_TENS)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isHoursTens - Current LCD is HOURS_TENS!");    
#endif
    //return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isHoursTens - Current LCD is NOT HOURS_TENS!");
#endif
    //return false;
  }
  //Second way to check if the current LCD is the seconds ones LCD
  // Check if the current LCD is the seconds ones LCD
  if (activOrLOWCSPinMask == (1 << lcdEnablePins[HOURS_TENS]))
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::isHoursTens - Current LCD is HOURS_TENS - 2nd way!");
    Serial.print("ChipSelect::isHoursTens - Finished with true!");
#endif
    return true;
  }
  else
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::isHoursTens - Current LCD is NOT HOURS_TENS - 2nd way!");
  Serial.println("ChipSelect::isHoursTens - Finished with false!");
#endif

    return false;
  }
  //return true;
#endif
}

void ChipSelect::enableAllCSPins()
{
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::enableAllCSPins - Enabling all CS pins...");
  Serial.print("ChipSelect::enableAllCSPins - allCSPinMask: ");
  Serial.println(allCSPinMask, BIN);
  Serial.print("ChipSelect::enableAllCSPins - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::enableAllCSPins - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
#ifdef HARDWARE_IPSTUBE_CLOCK
  // enable each LCD
  GPIO.out_w1tc = allCSPinMask; // Set all pins LOW (enable)
  delayMicroseconds(1);

  activOrLOWCSPinMask = allCSPinMask; // Set the active CS pin mask to all pins (all enabled)
  inactiveOrHIGHCSPinMask = 0; // Set the inactive CS pin mask to all pins (all disabled)

#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::enableAllCSPins - All CS pins enabled");
  Serial.print("ChipSelect::enableAllCSPinMask: - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::enableAllCSPinMask: - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
  Serial.println("ChipSelect::enableAllCSPins - Finished: ");
#endif
#endif
}

void ChipSelect::disableAllCSPins()
{
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::disableAllCSPins - Disabling all CS pins...");
  Serial.print("ChipSelect::disableAllCSPinMask: - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::disableAllCSPinMask: - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
#ifdef HARDWARE_IPSTUBE_CLOCK
  // disable each LCD

  GPIO.out_w1ts = allCSPinMask; // Set all pins HIGH (disable)
  delayMicroseconds(1);

  activOrLOWCSPinMask = 0; // Set the active CS pin mask to all pins (all disabled)
  inactiveOrHIGHCSPinMask = allCSPinMask; // Set the inactive CS pin mask to all pins (all enabled)

#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::disableAllCSPins - All CS pins disabled");
  Serial.print("ChipSelect::disableAllCSPinMask: - activOrLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::disableAllCSPinMask: - inactiveOrHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
  Serial.println("ChipSelect::disableAllCSPins - Finished: ");
#endif
#endif
}

void ChipSelect::enableDigitCSPins(uint8_t digit)
{
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::enableDigitCSPins - Enabling digit: ");
  Serial.println(digit);
  Serial.print("ChipSelect::enableDigitCSPins - Pin: ");
  Serial.println(lcdEnablePins[digit]);  
  Serial.print("ChipSelect::enableDigitCSPins - activorLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::enableDigitCSPins - inactiveorHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
#ifdef HARDWARE_IPSTUBE_CLOCK
  // Set the pin LOW (enable)

  // Check if the pin is already LOW (enabled)
  if ((GPIO.out & (1 << lcdEnablePins[digit])) != 0) // Pin is HIGH
  {
    GPIO.out_w1tc = (1 << lcdEnablePins[digit]); // Set LOW
    delayMicroseconds(1); // Wait a bit to ensure the pin is set LOW

    //add the activated pin to the activeorLOWCSpinmask
    activOrLOWCSPinMask |= (1 << lcdEnablePins[digit]); // Set the current LCD pin LOW (enable)
    //subtract the acivated pin from the inactiveorHIGHCSpinmask
    inactiveOrHIGHCSPinMask &= ~(1 << lcdEnablePins[digit]); // Set the current LCD pin LOW (enable)

#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.print("ChipSelect::enableDigitCSPins - Pin was HIGH (disabled), now set to LOW (enabled): ");
    Serial.println(activOrLOWCSPinMask, BIN);
    Serial.print("ChipSelect::enableDigitCSPinMask: - inactiveOrHIGHCSPinMask: ");
    Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
  }
#ifdef DEBUG_OUTPUT_CHIPSELECT
  else
  {
    Serial.println("ChipSelect::enableDigitCSPins - Pin is already LOW (enabled)!");
    Serial.print("ChipSelect::enableDigitCSPinMask: - activOrLOWCSPinMask: ");
    Serial.println(activOrLOWCSPinMask, BIN);
    Serial.print("ChipSelect::enableDigitCSPinMask: - inactiveOrHIGHCSPinMask: ");
    Serial.println(inactiveOrHIGHCSPinMask, BIN);
  }
  Serial.println("ChipSelect::enableDigitCSPins - Finished!");
#endif
#endif
}

void ChipSelect::disableDigitCSPins(uint8_t digit)
{
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::disableDigitCSPins - Disabling digit: ");
  Serial.println(digit);
  Serial.print("ChipSelect::disableDigitCSPins - Pin: ");
  Serial.println(lcdEnablePins[digit]);
  Serial.print("ChipSelect::disableDigitCSPins - activorLOWCSPinMask: ");
  Serial.println(activOrLOWCSPinMask, BIN);
  Serial.print("ChipSelect::disableDigitCSPinMask: - inactiveorHIGHCSPinMask: ");
  Serial.println(inactiveOrHIGHCSPinMask, BIN);
#endif
#ifdef HARDWARE_IPSTUBE_CLOCK
  // Set the pin HIGH (disable)
  // Check if the pin is LOW (enabled)
  if ((GPIO.out & (1 << lcdEnablePins[digit])) == 0) // Pin is LOW
  {
    GPIO.out_w1ts = (1 << lcdEnablePins[digit]); // Set HIGH
    delayMicroseconds(1); // Wait a bit to ensure the pin is set HIGH

    //subtract the deacivated pin from the activeorLOWCSpinmask
    activOrLOWCSPinMask &= ~(1 << lcdEnablePins[digit]); // Set the current LCD pin HIGH (disable)
    //add the deactivated pin to the inactiveorHIGHCSpinmask
    inactiveOrHIGHCSPinMask |= (1 << lcdEnablePins[digit]); // Set the current LCD pin HIGH (disable))
  }
#ifdef DEBUG_OUTPUT_CHIPSELECT
  else
  {
    Serial.println("ChipSelect::disableDigitCSPins - Pin is already HIGH (disabled)!");
    Serial.print("ChipSelect::disableDigitCSPinMask: - activOrLOWCSPinMask: ");
    Serial.println(activOrLOWCSPinMask, BIN);
    Serial.print("ChipSelect::disableDigitCSPinMask: - inactiveOrHIGHCSPinMask: ");
    Serial.println(inactiveOrHIGHCSPinMask, BIN);
  }
  Serial.println("ChipSelect::disableDigitCSPins - Finished!");
#endif
#endif
}
