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

#ifdef HARDWARE_MARVELTUBES_CLOCK
// Define the pins for each LCD's enable wire
const int lcdEnablePins[NUM_DIGITS] = {37, 36, 35, 34, 33, 15}; // GPIO pins for each LCD CS
const int numLCDs = NUM_DIGITS;
#endif

void ChipSelect::begin()
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::begin - Initializing shift-register based chip select");
#endif
  pinMode(CSSR_LATCH_PIN, OUTPUT);
  pinMode(CSSR_DATA_PIN, OUTPUT);
  pinMode(CSSR_CLOCK_PIN, OUTPUT);

#ifdef HARDWARE_XUNFENG_CLOCK
  // Xunfeng clock shift register seems to need some pins to be set to LOW or HIGH at the beginning, otherwise it will not work correctly
  // Initialize the shift register pins
  pinMode(GPIO_NUM_14, OUTPUT);
  pinMode(GPIO_NUM_17, OUTPUT);

  digitalWrite(GPIO_NUM_14, LOW);  // Set this pin to LOW
  digitalWrite(GPIO_NUM_17, HIGH); // Set this latch pin to HIGH
#endif                             // HARDWARE_XUNFENG_CLOCK

  digitalWrite(CSSR_DATA_PIN, LOW);
  digitalWrite(CSSR_CLOCK_PIN, LOW);
  digitalWrite(CSSR_LATCH_PIN, LOW);
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::begin - Shift register pins prepared, issuing initial update()");
#endif
  update();
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::begin - Initializing per-digit CS pins");
#endif
  // Initialize all six different pins for the CS of each LCD as OUTPUT and set it to DIGIT_CS_INACTIVE_LEVEL (disabled)
  for (int i = 0; i < numLCDs; ++i)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.print("ChipSelect::begin - Config pin ");
    Serial.print(lcdEnablePins[i]);
    Serial.println(" as OUTPUT, default disabled");
#endif
    pinMode(lcdEnablePins[i], OUTPUT);
    digitalWrite(lcdEnablePins[i], DIGIT_CS_INACTIVE_LEVEL);
  }
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::begin - All CS pins set to DIGIT_CS_INACTIVE_LEVEL (disabled)");
  Serial.println("ChipSelect::begin - DIGIT_CS_INACTIVE_LEVEL is: ");
  Serial.println(DIGIT_CS_INACTIVE_LEVEL);
  Serial.println("ChipSelect::begin - DIGIT_CS_ACTIVE_LEVEL is: ");
  Serial.println(DIGIT_CS_ACTIVE_LEVEL);
  Serial.println("ChipSelect::begin - Finished!");
#endif
#endif
}

void ChipSelect::clear(bool update_)
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::clear - Clearing shift register state");
#endif
  setDigitMap(all_off, update_);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::clear - Disabling all per-digit CS pins");
#endif
  disableAllCSPins();
#endif
}

void ChipSelect::setAll(bool update_)
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::setAll - Enabling all digits via shift register");
#endif
  setDigitMap(all_on, update_);
#else
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::setAll - Enabling all per-digit CS pins");
#endif
  enableAllCSPins();
#endif
}

void ChipSelect::setDigit(uint8_t digit, bool update_)
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
  // Set the bit for the given digit in the digits_map
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::setDigit - Selecting digit ");
  Serial.println(digit);
#endif
  setDigitMap(1 << digit, update_);
  if (update_)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.println("ChipSelect::setDigit - update_ requested, calling update()");
#endif
    update();
  }
#else
  // Set the actual currentLCD value for the given digit and activate the corresponding LCD

  // first deactivate the current LCD
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::setDigit - Switching from digit ");
  Serial.print(currentLCD);
  Serial.print(" to digit ");
  Serial.println(digit);
#endif
  disableDigitCSPins(currentLCD);
  // store the current
  currentLCD = digit;
  // activate the new one
  enableDigitCSPins(digit);
  // NO UPDATE, cause update enables and disables the pin, but needs to be "enabled", while eTFT_SPI is writing into it.
#endif
}

void ChipSelect::update()
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
  // Documented in README.md.  Q7 and Q6 are unused. Q5 is Seconds Ones, Q0 is Hours Tens.
  // Q7 is the first bit written, Q0 is the last.  So we push two dummy bits, then start with
  // Seconds Ones and end with Hours Tens.
  // CS is Active Low, but digits_map is 1 for enable, 0 for disable.  So we bit-wise NOT first.

  uint8_t to_shift = (~digits_map) << 2;

#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::update - Shifting map 0b");
  Serial.println(to_shift, BIN);
#endif
  digitalWrite(CSSR_LATCH_PIN, LOW);
  shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, LSBFIRST, to_shift);
  digitalWrite(CSSR_LATCH_PIN, HIGH);
#else
  // this is just, to follow the "update" logic of the other hardware!
  // for IPSTUBE clocks, the CS pin is already pulled to LOW by the "setDigit" function and stays there, till another "setDigit" is called.
  // so all writing done by the eTFT_SPI lib functions in the time, the pin is low, will write out directly to the LCD.
  //"Update" never will work, because, if pin was HIGH, no writing was done.
  digitalWrite(lcdEnablePins[currentLCD], DIGIT_CS_ACTIVE_LEVEL);
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::update - Maintaining digit ");
  Serial.print(currentLCD);
  Serial.print(" active on pin ");
  Serial.println(lcdEnablePins[currentLCD]);
#endif
#endif
}

bool ChipSelect::isSecondsOnes()
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
  return ((digits_map & SECONDS_ONES_MAP) > 0);
#else
  return true;
#endif
}

bool ChipSelect::isSecondsTens()
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
  return ((digits_map & SECONDS_TENS_MAP) > 0); 
#else
  return true;
#endif
}

bool ChipSelect::isMinutesOnes()
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
  return ((digits_map & MINUTES_ONES_MAP) > 0);
#else
  return true;
#endif
}

bool ChipSelect::isMinutesTens()
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
  return ((digits_map & MINUTES_TENS_MAP) > 0);
#else
  return true;
#endif
}

bool ChipSelect::isHoursOnes()
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
  return ((digits_map & HOURS_ONES_MAP) > 0);
#else
  return true;
#endif
}

bool ChipSelect::isHoursTens()
{
#if (!defined(HARDWARE_IPSTUBE_CLOCK) && !defined(HARDWARE_MARVELTUBES_CLOCK))
  return ((digits_map & HOURS_TENS_MAP) > 0);
#else
  return true;
#endif
}

void ChipSelect::enableAllCSPins()
{
#if (defined(HARDWARE_IPSTUBE_CLOCK) || defined(HARDWARE_MARVELTUBES_CLOCK))
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::enableAllCSPins - Enabling every digit");
#endif
  // enable each LCD
  for (int i = 0; i < numLCDs; ++i)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.print("ChipSelect::enableAllCSPins - Pin ");
    Serial.print(lcdEnablePins[i]);
    Serial.println(" -> ACTIVATEDISPLAYS");
#endif
    digitalWrite(lcdEnablePins[i], DIGIT_CS_ACTIVE_LEVEL);
  }
#endif
}

void ChipSelect::disableAllCSPins()
{
#if (defined(HARDWARE_IPSTUBE_CLOCK) || defined(HARDWARE_MARVELTUBES_CLOCK))
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.println("ChipSelect::disableAllCSPins - Disabling every digit");
#endif
  // disable each LCD
  for (int i = 0; i < numLCDs; ++i)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.print("ChipSelect::disableAllCSPins - Pin ");
    Serial.print(lcdEnablePins[i]);
    Serial.println(" -> DEACTIVATEDISPLAYS");
#endif
    digitalWrite(lcdEnablePins[i], DIGIT_CS_INACTIVE_LEVEL);
  }
#endif
}

void ChipSelect::enableDigitCSPins(uint8_t digit)
{
#if (defined(HARDWARE_IPSTUBE_CLOCK) || defined(HARDWARE_MARVELTUBES_CLOCK))
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::enableDigitCSPins - Digit ");
  Serial.print(digit);
  Serial.print(" pin ");
  Serial.print(lcdEnablePins[digit]);
  Serial.println(" -> ACTIVATEDISPLAYS");
#endif
  // enable the LCD for the given digit
  digitalWrite(lcdEnablePins[digit], DIGIT_CS_ACTIVE_LEVEL);
#endif
}

void ChipSelect::disableDigitCSPins(uint8_t digit)
{
#if (defined(HARDWARE_IPSTUBE_CLOCK) || defined(HARDWARE_MARVELTUBES_CLOCK))
#ifdef DEBUG_OUTPUT_CHIPSELECT
  Serial.print("ChipSelect::disableDigitCSPins - Digit ");
  Serial.print(digit);
  Serial.print(" pin ");
  Serial.print(lcdEnablePins[digit]);
  Serial.println(" -> DEACTIVATEDISPLAYS");
#endif
  // disable the LCD for the given digit
  digitalWrite(lcdEnablePins[digit], DIGIT_CS_INACTIVE_LEVEL);
#endif
}
