#include "ChipSelect.h"

#ifdef HARDWARE_IPSTUBE_CLOCK
// Define the pins for each LCD's enable wire
// The order is from left to right, so the first pin is for the seconds ones, the last for the hours tens
// LCD2 is the leftmost one         - seconds one - pin 21 as GPIO15
// LCD3 is the second from the left - seconds ten - pin 22 as GPIO2
// LCD4 is the third from the left  - minutes one - pin 23 as GPIO27
// LCD5 is the fourth from the left - minutes ten - pin 17 as GPIO14
// LCD6 is the fifth from the left  - hours one   - pin 18 as GPIO12
// LCD7 is the rightmost one        - hours ten   - pin 20 as GPIO13
const int lcdEnablePins[NUM_DIGITS] = {GPIO_NUM_15, GPIO_NUM_2, GPIO_NUM_27, GPIO_NUM_14, GPIO_NUM_12, GPIO_NUM_13};
const int numLCDs = NUM_DIGITS;
#endif

void ChipSelect::begin()
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  pinMode(CSSR_LATCH_PIN, OUTPUT);
  pinMode(CSSR_DATA_PIN, OUTPUT);
  pinMode(CSSR_CLOCK_PIN, OUTPUT);

  digitalWrite(CSSR_DATA_PIN, LOW);
  digitalWrite(CSSR_CLOCK_PIN, LOW);
  digitalWrite(CSSR_LATCH_PIN, LOW);
  update();
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::begin - Initializing ChipSelect...");
#endif
  // Initialize all six different pins for the CS of each LCD as OUTPUT and set it to HIGH (disabled)
  for (int i = 0; i < numLCDs; ++i)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.print("ChipSelect::begin - Initializing pin: ");
    Serial.print(lcdEnablePins[i]);
    Serial.print(" as OUTPUT and set to HIGH (disabled)");
#endif
    pinMode(lcdEnablePins[i], OUTPUT);
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.print("ChipSelect::begin - calling digitalWrite() with HIGH (disabled) for pin: ");
    Serial.println(lcdEnablePins[i]);
#endif
    digitalWrite(lcdEnablePins[i], HIGH);
  }
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::begin - Initialized ALL CS pins as OUTPUT and set to HIGH (disabled)");
  Serial.println("ChipSelect::begin - Finished!");
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
#endif
  disableAllCSPins();
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::clear - All CS pins disabled");
  Serial.println("ChipSelect::clear - Finished!");
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
  Serial.println("ChipSelect::setAll - Finished!");
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
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::setDigit - digit to activate: ");
  Serial.println(digit);
  Serial.print("ChipSelect::setDigit - Selected digit from last run or elsewhere ('old' currentLCD): ");
  Serial.println(currentLCD);
#endif
  // Set the actual currentLCD value for the given digit and activate the corresponding LCD

  // first deactivate the current LCD
  disableDigitCSPins(currentLCD);
  // store the current
  currentLCD = digit;
  // activate the new one
  enableDigitCSPins(digit);
  // NO UPDATE, cause update enables and disables the pin, but needs to be "enabled", while eTFT_SPI is writing into it.
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::setDigit - Activated digit ('new' currentLCD): ");
  Serial.println(digit);
  Serial.println("ChipSelect::setDigit - Finished!");
#endif
#endif
}

void ChipSelect::update()
{
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
  Serial.print("ChipSelect::update - Calling digitalWrite() with HIGH (disabled) for pin: ");
  Serial.println(lcdEnablePins[currentLCD]);  
#endif
  digitalWrite(lcdEnablePins[currentLCD], LOW);
#ifdef DEBUG_OUTPUT_CHIPSELECT
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
#endif
  return true;
#endif
}

bool ChipSelect::isSecondsTens()
{
#ifndef HARDWARE_IPSTUBE_CLOCK
  return ((digits_map & SECONDS_TENS_MAP) > 0);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::isSecondsTens - Current LCD: ");
  Serial.println(currentLCD);
#endif
  return true;
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
#endif
  return true;
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
#endif
  return true;
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
#endif
  return true;
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
#endif
  return true;
#endif
}

void ChipSelect::enableAllCSPins()
{
#ifdef HARDWARE_IPSTUBE_CLOCK
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::enableAllCSPins - Enabling all CS pins...");
#endif
  // enable each LCD
  for (int i = 0; i < numLCDs; ++i)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.print("ChipSelect::enableAllCSPins - calling digitalWrite() with LOW (enabled) for pin: ");
    Serial.println(lcdEnablePins[i]);
#endif
    digitalWrite(lcdEnablePins[i], LOW);
  }
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::enableAllCSPins - All CS pins enabled");
  Serial.println("ChipSelect::enableAllCSPins - Finished!");
#endif
#endif
}

void ChipSelect::disableAllCSPins()
{
#ifdef HARDWARE_IPSTUBE_CLOCK
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::disableAllCSPins - Disabling all CS pins...");
#endif
  // disable each LCD
  for (int i = 0; i < numLCDs; ++i)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.print("ChipSelect::disableAllCSPins - calling digitalWrite() with HIGH (disabled) for pin: ");
    Serial.println(lcdEnablePins[i]);
#endif
    digitalWrite(lcdEnablePins[i], HIGH);
  }
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::disableAllCSPins - All CS pins disabled");
  Serial.println("ChipSelect::disableAllCSPins - Finished!");
#endif
#endif
}

void ChipSelect::enableDigitCSPins(uint8_t digit)
{
#ifdef HARDWARE_IPSTUBE_CLOCK
#ifdef DEBUG_OUTPUT_CHIPSELECT
Serial.print("ChipSelect::enableDigitCSPins - Digit to enable: ");
Serial.println(digit);
Serial.print("ChipSelect::enableDigitCSPins - Current LCD: ");
Serial.println(currentLCD);
Serial.print("ChipSelect::enableDigitCSPins - calling digitalWrite() with LOW (enabled) for pin: ");
Serial.println(lcdEnablePins[digit]);
#endif
  // enable the LCD for the given digit
  digitalWrite(lcdEnablePins[digit], LOW);
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::enableDigitCSPins - Finished!");
#endif
#endif
}

void ChipSelect::disableDigitCSPins(uint8_t digit)
{
#ifdef HARDWARE_IPSTUBE_CLOCK
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::disableDigitCSPins - Digit to disable: ");
  Serial.println(digit);
  Serial.print("ChipSelect::disableDigitCSPins - Current LCD: ");
  Serial.println(currentLCD);
  Serial.print("ChipSelect::disableDigitCSPins - calling digitalWrite() with HIGH (disabled) for pin: ");
  Serial.println(lcdEnablePins[digit]);
#endif
  // disable the LCD for the given digit
  digitalWrite(lcdEnablePins[digit], HIGH);
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::disableDigitCSPins - Finished!");
#endif
#endif
}
