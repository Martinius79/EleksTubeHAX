/*
 * Author: Aljaz Ogrin
 * Project: Alternative firmware for EleksTube IPS clock
 * Original location: https://github.com/aly-fly/EleksTubeHAX
 * Hardware: ESP32
 * Based on: https://github.com/SmittyHalibut/EleksTubeHAX
 */

#include <nvs_flash.h>
#include <stdint.h>
#include <math.h>
#include <Wire.h>
#include <SPI.h>
#include "GLOBAL_DEFINES.h"
#include "Backlights.h"
#include "Buttons.h"
#include "Clock.h"
#include "Menu.h"
#include "StoredConfig.h"
#include "TFTs.h"
#include "WiFi_WPS.h"

#ifdef GEOLOCATION_ENABLED
#include "IPGeolocation_AO.h"
#endif

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
#include "MQTT_client_ips.h"
#endif

/************************
 *   NovelLife Clone    *
 ************************/
#ifdef HARDWARE_NOVELLIFE_CLOCK
// #include "Gestures.h"
// TODO put into class
#include <Wire.h>
#include <SparkFun_APDS9960.h>

// TODO put into class.
SparkFun_APDS9960 apds = SparkFun_APDS9960();
int volatile isr_flag = 0; // Interrupt signal for gesture sensor

extern Buttons buttons;

void GestureStart();
void HandleGestureInterupt(void);
void GestureInterruptRoutine(void);
void HandleGesture(void);

void GestureStart()
{
  // For gesture sensor APDS9660 set interrupt pin on ESP32 as input.
  pinMode(GESTURE_SENSOR_INPUT_PIN, INPUT);

  // Initialize interrupt service routine for APDS-9960 sensor.
  attachInterrupt(digitalPinToInterrupt(GESTURE_SENSOR_INPUT_PIN), GestureInterruptRoutine, FALLING);

  // Initialize gesture sensor APDS-9960 (configure I2C and initial values).
  if (apds.init())
  {
    Serial.println(F("APDS-9960 initialization complete"));

    // Set Gain to 1x, because the cheap chinese fake APDS sensor can't handle more (also remember to extend ID check in SparkFun libary to 0x3B!).
    apds.setGestureGain(GGAIN_1X);

    // Start running the APDS-9960 gesture sensor engine.
    if (apds.enableGestureSensor(true))
    {
      Serial.println(F("Gesture sensor is now running"));
    }
    else
    {
      Serial.println(F("Something went wrong during gesture sensor enablimg in the APDS-9960 library!"));
    }
  }
  else
  {
    Serial.println(F("Something went wrong during APDS-9960 init!"));
  }
}

// Handle Interrupt from gesture sensor and simulate a short button press (state down_edge) of the corresponding button, if a gesture is detected.
void HandleGestureInterupt()
{
  if (isr_flag == 1)
  {
    detachInterrupt(digitalPinToInterrupt(GESTURE_SENSOR_INPUT_PIN));
    HandleGesture();
    isr_flag = 0;
    attachInterrupt(digitalPinToInterrupt(GESTURE_SENSOR_INPUT_PIN), GestureInterruptRoutine, FALLING);
  }
  return;
}

// Mark that the Interrupt of the gesture sensor was signaled.
void GestureInterruptRoutine()
{
  isr_flag = 1;
  return;
}

// Check which gesture was detected.
void HandleGesture()
{
  // Serial.println("->main::HandleGesture()");
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
  return;
}
#endif // #ifdef HARDWARE_NOVELLIFE_CLOCK

char UniqueDeviceName[32];      // Enough space for <DeviceName> + 6 hex chars + null

Backlights backlights;
Buttons buttons;
TFTs tfts;
Clock uclock;
Menu menu;
StoredConfig stored_config;

#ifdef GEOLOCATION_ENABLED
double GeoLocTZoffset = 0;
bool GetGeoLocationTimeZoneOffset();
constexpr uint8_t GEOLOC_MAX_FAILURES_PER_DAY = 4;
constexpr uint32_t GEOLOC_RETRY_BACKOFF_MS = 5UL * 60UL * 1000UL;
uint8_t GeoLocFailedAttempts = 0;
uint32_t GeoLocNextRetryMillis = 0;
uint8_t GeoLocAttemptDay = 0;
bool GeoLocNeedsUpdate = false;
void processGeoLocUpdate(void);
void checkUpdateGeoLocNeeded(void);
uint8_t yesterday = 0;
#endif

#ifdef DIMMING
bool isDimmingNeeded = false;
uint8_t hour_old = 255;
#endif

uint32_t lastMQTTCommandExecuted = (uint32_t)-1;
static constexpr uint8_t EXPANDER_ADDR = 0x19;
static bool expander_present = false;
static TFT_eSPI test_tft;

// Helper function, defined below.
void updateClockDisplay(TFTs::show_t show = TFTs::yes);
void setupMenu(void);
#ifdef DIMMING
bool isNightTime(uint8_t current_hour);
void checkDimmingNeeded(void);
#endif

static bool i2cReadReg(uint8_t address, uint8_t reg, uint8_t &value)
{
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }
  if (Wire.requestFrom(address, (uint8_t)1) != 1)
  {
    return false;
  }
  value = Wire.read();
  return true;
}

static bool i2cReadDirect(uint8_t address, uint8_t &value)
{
  if (Wire.requestFrom(address, (uint8_t)1) != 1)
  {
    return false;
  }
  value = Wire.read();
  return true;
}

static bool i2cWriteReg(uint8_t address, uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool expanderWriteCmd(uint8_t address, uint8_t cmd, uint8_t arg)
{
  return i2cWriteReg(address, cmd, arg);
}

static void spiWriteCommand(uint8_t cmd)
{
  digitalWrite(TFT_DC, LOW);
  SPI.transfer(cmd);
}

static void spiWriteData(uint8_t data)
{
  digitalWrite(TFT_DC, HIGH);
  SPI.transfer(data);
}

static void spiWriteData16(uint16_t data)
{
  digitalWrite(TFT_DC, HIGH);
  SPI.transfer((uint8_t)(data >> 8));
  SPI.transfer((uint8_t)(data & 0xFF));
}

static uint16_t tftOffsetX()
{
#if defined(CGRAM_OFFSET)
  return 26;
#else
  return 0;
#endif
}

static uint16_t tftOffsetY()
{
#if defined(CGRAM_OFFSET)
  return 1;
#else
  return 0;
#endif
}

static void tftInitSt7735()
{
#if (TFT_RST >= 0)
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(20);
  digitalWrite(TFT_RST, HIGH);
  delay(120);
#endif

  spiWriteCommand(0x01); // SWRESET
  delay(150);
  spiWriteCommand(0x11); // SLPOUT
  delay(120);
  spiWriteCommand(0xB1); // FRMCTR1
  spiWriteData(0x01);
  spiWriteData(0x2C);
  spiWriteData(0x2D);
  spiWriteCommand(0xB2); // FRMCTR2
  spiWriteData(0x01);
  spiWriteData(0x2C);
  spiWriteData(0x2D);
  spiWriteCommand(0xB3); // FRMCTR3
  spiWriteData(0x01);
  spiWriteData(0x2C);
  spiWriteData(0x2D);
  spiWriteData(0x01);
  spiWriteData(0x2C);
  spiWriteData(0x2D);
  spiWriteCommand(0xB4); // INVCTR
  spiWriteData(0x07);
  spiWriteCommand(0xC0); // PWCTR1
  spiWriteData(0xA2);
  spiWriteData(0x02);
  spiWriteData(0x84);
  spiWriteCommand(0xC1); // PWCTR2
  spiWriteData(0xC5);
  spiWriteCommand(0xC2); // PWCTR3
  spiWriteData(0x0A);
  spiWriteData(0x00);
  spiWriteCommand(0xC3); // PWCTR4
  spiWriteData(0x8A);
  spiWriteData(0x2A);
  spiWriteCommand(0xC4); // PWCTR5
  spiWriteData(0x8A);
  spiWriteData(0xEE);
  spiWriteCommand(0xC5); // VMCTR1
  spiWriteData(0x0E);
  spiWriteCommand(0x20); // INVOFF
  spiWriteCommand(0x36); // MADCTL
  spiWriteData(0xC8);
  spiWriteCommand(0x3A); // COLMOD
  spiWriteData(0x05);    // 16-bit color
  spiWriteCommand(0x29); // DISPON
  delay(20);
}

static void tftSetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  const uint16_t x_off = tftOffsetX();
  const uint16_t y_off = tftOffsetY();
  spiWriteCommand(0x2A); // CASET
  spiWriteData16((uint16_t)(x0 + x_off));
  spiWriteData16((uint16_t)(x1 + x_off));
  spiWriteCommand(0x2B); // RASET
  spiWriteData16((uint16_t)(y0 + y_off));
  spiWriteData16((uint16_t)(y1 + y_off));
  spiWriteCommand(0x2C); // RAMWR
}

static void tftFillColor(uint16_t color)
{
  tftSetWindow(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
  uint32_t count = (uint32_t)TFT_WIDTH * (uint32_t)TFT_HEIGHT;
  while (count--)
  {
    spiWriteData16(color);
  }
}

static inline uint16_t tftInvertColor(uint16_t color)
{
  return (uint16_t)(color ^ 0xFFFF);
}

static bool i2cReadRegs(uint8_t address, uint8_t startReg, uint8_t *buffer, uint8_t length)
{
  Wire.beginTransmission(address);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }
  if (Wire.requestFrom(address, length) != length)
  {
    return false;
  }
  for (uint8_t i = 0; i < length; i++)
  {
    buffer[i] = Wire.read();
  }
  return true;
}

static bool expanderSendCmd(uint8_t address, uint8_t cmd, uint8_t arg, uint8_t *resp, uint8_t resp_len)
{
  Wire.beginTransmission(address);
  Wire.write(cmd);
  Wire.write(arg);
  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }
  uint8_t len = Wire.requestFrom(address, resp_len);
  if (len != resp_len)
  {
    return false;
  }
  for (uint8_t i = 0; i < resp_len; i++)
  {
    resp[i] = Wire.read();
  }
  return true;
}

static bool expanderSetNibble(uint8_t value)
{
  uint8_t resp[4] = {0};
  return expanderSendCmd(EXPANDER_ADDR, 0x01, (uint8_t)(value & 0x0F), resp, 4);
}

static bool expanderReadStatus(uint8_t *resp4)
{
  return expanderSendCmd(EXPANDER_ADDR, 0x00, 0x00, resp4, 4);
}

static bool i2cSendCmdRead(uint8_t address, uint8_t cmd, uint8_t arg, uint8_t *resp, uint8_t resp_len)
{
  Wire.beginTransmission(address);
  Wire.write(cmd);
  Wire.write(arg);
  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }
  uint8_t len = Wire.requestFrom(address, resp_len);
  if (len != resp_len)
  {
    return false;
  }
  for (uint8_t i = 0; i < resp_len; i++)
  {
    resp[i] = Wire.read();
  }
  return true;
}

static void i2cProbeExpander(uint8_t address)
{
  Serial.println("  Probe: register dump 0x00..0x07");
  for (uint8_t reg = 0; reg <= 0x07; reg++)
  {
    uint8_t value = 0;
    if (i2cReadReg(address, reg, value))
    {
      Serial.printf("    Reg 0x%02X: 0x%02X\n", reg, value);
    }
  }

  uint8_t burst[4] = {0};
  if (i2cReadRegs(address, 0x00, burst, 4))
  {
    Serial.printf("  Probe: burst read @0x00: %02X %02X %02X %02X\n", burst[0], burst[1], burst[2], burst[3]);
  }
  if (i2cReadRegs(address, 0x01, burst, 4))
  {
    Serial.printf("  Probe: burst read @0x01: %02X %02X %02X %02X\n", burst[0], burst[1], burst[2], burst[3]);
  }

  // PCA/TCA9554-style tests (0x00 input, 0x01 output, 0x02 polarity, 0x03 config)
  uint8_t input_before = 0;
  uint8_t output_before = 0;
  uint8_t polarity_before = 0;
  uint8_t config_before = 0;

  i2cReadReg(address, 0x00, input_before);
  i2cReadReg(address, 0x01, output_before);
  i2cReadReg(address, 0x02, polarity_before);
  i2cReadReg(address, 0x03, config_before);

  Serial.printf("  Probe: input 0x%02X, output 0x%02X, polarity 0x%02X, config 0x%02X\n",
                input_before, output_before, polarity_before, config_before);

  // Polarity test: invert inputs (if supported) and compare
  if (i2cWriteReg(address, 0x02, (uint8_t)~polarity_before))
  {
    uint8_t input_after = 0;
    i2cReadReg(address, 0x00, input_after);
    Serial.printf("  Probe: input after polarity invert: 0x%02X\n", input_after);
    i2cWriteReg(address, 0x02, polarity_before);
  }

  // Config test: force outputs then restore
  if (i2cWriteReg(address, 0x03, 0x00))
  {
    uint8_t cfg_check = 0;
    i2cReadReg(address, 0x03, cfg_check);
    Serial.printf("  Probe: config forced to 0x00, readback 0x%02X\n", cfg_check);

    // Output write/readback test
    uint8_t out_test = (uint8_t)~output_before;
    if (i2cWriteReg(address, 0x01, out_test))
    {
      uint8_t out_rb = 0;
      i2cReadReg(address, 0x01, out_rb);
      Serial.printf("  Probe: output write 0x%02X, readback 0x%02X\n", out_test, out_rb);
      i2cWriteReg(address, 0x01, output_before);
    }

    i2cWriteReg(address, 0x03, config_before);
  }
}

static void i2cRegisterSweep(uint8_t address)
{
  static bool sweep_done = false;
  if (sweep_done)
  {
    return;
  }
  sweep_done = true;

  Serial.println("  Sweep: probing writable registers 0x00..0x7F");
  const uint8_t test_values[2] = {0x00, 0xFF};

  for (uint8_t reg = 0x00; reg <= 0x7F; reg++)
  {
    uint8_t before = 0;
    if (!i2cReadReg(address, reg, before))
    {
      continue;
    }

    bool any_change = false;
    uint8_t after_last = before;

    for (uint8_t i = 0; i < 2; i++)
    {
      uint8_t val = test_values[i];
      if (!i2cWriteReg(address, reg, val))
      {
        continue;
      }
      uint8_t after = before;
      if (i2cReadReg(address, reg, after) && after == val && after != before)
      {
        any_change = true;
        after_last = after;
      }
      // restore best-effort
      i2cWriteReg(address, reg, before);
    }

    if (any_change)
    {
      Serial.printf("  Sweep: reg 0x%02X writable (before 0x%02X, after 0x%02X)\n", reg, before, after_last);
    }
  }
}

static void i2cReplayInitSequence(uint8_t address)
{
  static bool replay_done = false;
  if (replay_done)
  {
    return;
  }
  replay_done = true;

  Serial.println("  Replay: init sequence 01 FE -> 01 FC -> 01 FE");
  i2cWriteReg(address, 0x01, 0xFE);
  delay(90);
  i2cWriteReg(address, 0x01, 0xFC);
  delay(90);
  i2cWriteReg(address, 0x01, 0xFE);
  delay(100);

  Serial.println("  Replay: burst toggle 00 18 / 00 FF (33x)");
  for (uint8_t i = 0; i < 33; i++)
  {
    i2cWriteReg(address, 0x00, 0x18);
    i2cWriteReg(address, 0x00, 0xFF);
    // delayMicroseconds(500);
  }
}

static void i2cCommandProbe(uint8_t address)
{
  static bool probe_done = false;
  if (probe_done)
  {
    return;
  }
  probe_done = true;

  Serial.println("  CmdProbe: 1-byte command scan (unique responses)");
  uint32_t last_sig = 0xFFFFFFFF;
  for (uint8_t cmd = 0x00; cmd <= 0x7F; cmd++)
  {
    Wire.beginTransmission(address);
    Wire.write(cmd);
    if (Wire.endTransmission(false) != 0)
    {
      continue;
    }

    uint8_t resp[4] = {0};
    uint8_t len = Wire.requestFrom(address, (uint8_t)4);
    if (len > 0)
    {
      for (uint8_t i = 0; i < len; i++)
      {
        resp[i] = Wire.read();
      }
      uint32_t sig = ((uint32_t)len << 24) | ((uint32_t)resp[0] << 16) | ((uint32_t)resp[1] << 8) | resp[2];
      if (sig != last_sig)
      {
        Serial.printf("  CmdProbe: cmd 0x%02X -> len %u: %02X %02X %02X %02X\n",
                      cmd, len, resp[0], resp[1], resp[2], resp[3]);
        last_sig = sig;
      }
    }
  }

  Serial.println("  CmdProbe: 2-byte command scan (subset) ");
  const uint8_t args[] = {0x00, 0x01, 0x02, 0x03, 0x0F, 0x55, 0xAA, 0xFF};
  for (uint8_t cmd = 0x00; cmd <= 0x7F; cmd++)
  {
    for (uint8_t a = 0; a < sizeof(args); a++)
    {
      Wire.beginTransmission(address);
      Wire.write(cmd);
      Wire.write(args[a]);
      if (Wire.endTransmission(false) != 0)
      {
        continue;
      }

      uint8_t resp[4] = {0};
      uint8_t len = Wire.requestFrom(address, (uint8_t)4);
      if (len > 0)
      {
        for (uint8_t i = 0; i < len; i++)
        {
          resp[i] = Wire.read();
        }
        uint32_t sig = ((uint32_t)len << 24) | ((uint32_t)resp[0] << 16) | ((uint32_t)resp[1] << 8) | resp[2];
        if (sig != last_sig)
        {
          Serial.printf("  CmdProbe2: cmd 0x%02X arg 0x%02X -> len %u: %02X %02X %02X %02X\n",
                        cmd, args[a], len, resp[0], resp[1], resp[2], resp[3]);
          last_sig = sig;
        }
      }
    }
  }
}

static void i2cCommandOutputTest(uint8_t address)
{
  static bool test_done = false;
  if (test_done)
  {
    return;
  }
  test_done = true;

  Serial.println("  CmdTest: sweep cmd=0x01 arg=0x0..0xF");
  for (uint8_t val = 0; val <= 0x0F; val++)
  {
    uint8_t resp_set[4] = {0};
    uint8_t resp_stat[4] = {0};
    uint8_t resp_rd[4] = {0};

    bool ok_set = i2cSendCmdRead(address, 0x01, val, resp_set, 4);
    bool ok_stat = i2cSendCmdRead(address, 0x00, 0x00, resp_stat, 4);
    bool ok_rd = i2cSendCmdRead(address, 0x01, 0x00, resp_rd, 4);

    uint8_t reg_in = 0;
    uint8_t reg_out = 0;
    i2cReadReg(address, 0x00, reg_in);
    i2cReadReg(address, 0x01, reg_out);

    Serial.printf("  CmdTest: set 0x%02X | set:%s %02X %02X %02X %02X | stat:%s %02X %02X %02X %02X | rd:%s %02X %02X %02X %02X | reg_in 0x%02X reg_out 0x%02X\n",
                  val,
                  ok_set ? "ok" : "--",
                  resp_set[0], resp_set[1], resp_set[2], resp_set[3],
                  ok_stat ? "ok" : "--",
                  resp_stat[0], resp_stat[1], resp_stat[2], resp_stat[3],
                  ok_rd ? "ok" : "--",
                  resp_rd[0], resp_rd[1], resp_rd[2], resp_rd[3],
                  reg_in, reg_out);
    delay(50);
  }
}

static void i2cCommandBankScan(uint8_t address)
{
  static bool scan_done = false;
  if (scan_done)
  {
    return;
  }
  scan_done = true;

  Serial.println("  CmdBankScan: cmd 0x00..0x0F with arg 0x00/0xFF");
  uint8_t base_in = 0;
  uint8_t base_out = 0;
  i2cReadReg(address, 0x00, base_in);
  i2cReadReg(address, 0x01, base_out);

  for (uint8_t cmd = 0x00; cmd <= 0x0F; cmd++)
  {
    for (uint8_t arg_i = 0; arg_i < 2; arg_i++)
    {
      uint8_t arg = arg_i == 0 ? 0x00 : 0xFF;
      uint8_t resp[4] = {0};
      if (!i2cSendCmdRead(address, cmd, arg, resp, 4))
      {
        continue;
      }
      uint8_t reg_in = 0;
      uint8_t reg_out = 0;
      i2cReadReg(address, 0x00, reg_in);
      i2cReadReg(address, 0x01, reg_out);

      bool resp_diff = (resp[0] != resp[1]) || (resp[0] != resp[2]) || (resp[0] != resp[3]);
      bool changed = (reg_in != base_in) || (reg_out != base_out) || resp_diff;
      if (changed)
      {
        Serial.printf("  CmdBankScan: cmd 0x%02X arg 0x%02X -> resp %02X %02X %02X %02X | reg_in 0x%02X reg_out 0x%02X\n",
                      cmd, arg, resp[0], resp[1], resp[2], resp[3], reg_in, reg_out);
      }
    }
  }
}

static void i2cCommandHighNibbleScan(uint8_t address)
{
  static bool scan_done = false;
  if (scan_done)
  {
    return;
  }
  scan_done = true;

  Serial.println("  CmdHiScan: try high-nibble args on cmd 0x01/0x02/0x03");
  const uint8_t cmds[] = {0x01, 0x02, 0x03};
  const uint8_t args[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0};
  for (uint8_t c = 0; c < sizeof(cmds); c++)
  {
    uint8_t cmd = cmds[c];
    for (uint8_t a = 0; a < sizeof(args); a++)
    {
      uint8_t arg = args[a];
      uint8_t resp[4] = {0};
      if (!i2cSendCmdRead(address, cmd, arg, resp, 4))
      {
        continue;
      }
      uint8_t reg_in = 0;
      uint8_t reg_out = 0;
      i2cReadReg(address, 0x00, reg_in);
      i2cReadReg(address, 0x01, reg_out);
      Serial.printf("  CmdHiScan: cmd 0x%02X arg 0x%02X -> resp %02X %02X %02X %02X | reg_in 0x%02X reg_out 0x%02X\n",
                    cmd, arg, resp[0], resp[1], resp[2], resp[3], reg_in, reg_out);
      delay(20);
    }
  }
}

static void i2cCommand3ByteScan(uint8_t address)
{
  static bool scan_done = false;
  if (scan_done)
  {
    return;
  }
  scan_done = true;

  Serial.println("  Cmd3Scan: cmd 0x00..0x0F with arg1/arg2 subset");
  const uint8_t args[] = {0x00, 0x01, 0x02, 0x03, 0x0F, 0x55, 0xAA, 0xFF};
  for (uint8_t cmd = 0x00; cmd <= 0x0F; cmd++)
  {
    for (uint8_t a = 0; a < sizeof(args); a++)
    {
      for (uint8_t b = 0; b < sizeof(args); b++)
      {
        Wire.beginTransmission(address);
        Wire.write(cmd);
        Wire.write(args[a]);
        Wire.write(args[b]);
        if (Wire.endTransmission(false) != 0)
        {
          continue;
        }
        uint8_t resp[4] = {0};
        uint8_t len = Wire.requestFrom(address, (uint8_t)4);
        if (len == 0)
        {
          continue;
        }
        for (uint8_t i = 0; i < len; i++)
        {
          resp[i] = Wire.read();
        }
        bool resp_diff = (resp[0] != resp[1]) || (resp[0] != resp[2]) || (resp[0] != resp[3]);
        if (resp_diff)
        {
          Serial.printf("  Cmd3Scan: cmd 0x%02X a 0x%02X b 0x%02X -> len %u: %02X %02X %02X %02X\n",
                        cmd, args[a], args[b], len, resp[0], resp[1], resp[2], resp[3]);
        }
      }
    }
  }
}

static void i2cUnlockSequenceTest(uint8_t address)
{
  static bool test_done = false;
  if (test_done)
  {
    return;
  }
  test_done = true;

  Serial.println("  UnlockTest: raw byte sequences + check cmd behavior");

  uint8_t base_in = 0;
  uint8_t base_out = 0;
  i2cReadReg(address, 0x00, base_in);
  i2cReadReg(address, 0x01, base_out);

  uint8_t base_resp[4] = {0};
  i2cSendCmdRead(address, 0x01, 0xF0, base_resp, 4);

  const uint8_t sequences[][2] = {
      {0xAA, 0x55},
      {0x55, 0xAA},
      {0x5A, 0xA5},
      {0xA5, 0x5A},
      {0xFF, 0x00},
      {0x00, 0xFF},
      {0x12, 0x34},
      {0xDE, 0xAD}};

  for (uint8_t i = 0; i < (sizeof(sequences) / sizeof(sequences[0])); i++)
  {
    Wire.beginTransmission(address);
    Wire.write(sequences[i][0]);
    Wire.write(sequences[i][1]);
    Wire.endTransmission();

    uint8_t reg_in = 0;
    uint8_t reg_out = 0;
    i2cReadReg(address, 0x00, reg_in);
    i2cReadReg(address, 0x01, reg_out);

    uint8_t resp1[4] = {0};
    uint8_t resp2[4] = {0};
    i2cSendCmdRead(address, 0x01, 0xF0, resp1, 4);
    i2cSendCmdRead(address, 0x01, 0x0F, resp2, 4);

    bool changed = (reg_in != base_in) || (reg_out != base_out) ||
                   memcmp(resp1, base_resp, sizeof(base_resp)) != 0;

    if (changed)
    {
      Serial.printf("  UnlockTest: seq %02X %02X -> reg_in 0x%02X reg_out 0x%02X | cmd01 F0: %02X %02X %02X %02X | cmd01 0F: %02X %02X %02X %02X\n",
                    sequences[i][0], sequences[i][1], reg_in, reg_out,
                    resp1[0], resp1[1], resp1[2], resp1[3],
                    resp2[0], resp2[1], resp2[2], resp2[3]);
    }
  }
}

static void i2cSoftResetRecover(uint8_t address)
{
  Serial.println("  SoftReset: attempting I2C recover");
  Wire.end();
  delay(10);
  Wire.begin(3, 4);
  Wire.setTimeOut(50);

  uint8_t resp[4] = {0};
  if (i2cSendCmdRead(address, 0x00, 0x00, resp, 4))
  {
    Serial.printf("  SoftReset: status after recover %02X %02X %02X %02X\n", resp[0], resp[1], resp[2], resp[3]);
  }
  else
  {
    Serial.println("  SoftReset: status read failed");
  }
}

static void i2cScan()
{
  uint8_t found = 0;
  Serial.println("I2C scan on SDA=GPIO4, SCL=GPIO3");
  for (uint8_t address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0)
    {
      Serial.printf("I2C device found at 0x%02X\n", address);
      found++;

      uint8_t value = 0;
      if (i2cReadReg(address, 0x00, value))
      {
        Serial.printf("  Reg 0x00 (Input): 0x%02X\n", value);
      }
      if (i2cReadReg(address, 0x01, value))
      {
        Serial.printf("  Reg 0x01 (Output): 0x%02X\n", value);
      }
      if (i2cReadReg(address, 0x02, value))
      {
        Serial.printf("  Reg 0x02 (Polarity): 0x%02X\n", value);
      }
      if (i2cReadReg(address, 0x03, value))
      {
        Serial.printf("  Reg 0x03 (Config): 0x%02X\n", value);
      }
      if (i2cReadDirect(address, value))
      {
        Serial.printf("  Direct read: 0x%02X\n", value);
      }
    }
    // else 
    // {
    //   // print errors
    //   if (error == 4)
    //     Serial.printf("Unknown error at address 0x%02X\n", address);
    //   else if (error == 2)
    //     Serial.printf("NACK on transmit of address 0x%02X\n", address);
    //   else if (error == 3)
    //     Serial.printf("NACK on transmit of data at address 0x%02X\n", address);
    //   else if (error == 1)
    //     Serial.printf("Other error at address 0x%02X\n", address);
    //     else
    //     Serial.printf("Error %d at address 0x%02X\n", error, address);
  
    // }

    delay(1);
  }
  if (found == 0)
  {
    Serial.println("No I2C devices found.");
  }
}

//-----------------------------------------------------------------------
// Setup
//-----------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(2000); // Wait for serial monitor to catch up
  Serial.println("\nSystem starting...\n");
  delay(100); // Wait for serial monitor to catch up

  Wire.begin(3,4);
  Wire.setTimeOut(50);
  i2cScan();

  Wire.beginTransmission(EXPANDER_ADDR);
  expander_present = (Wire.endTransmission() == 0);
  Serial.printf("Expander present at 0x%02X: %s\n", EXPANDER_ADDR, expander_present ? "yes" : "no");

  if (expander_present)
  {
    expanderWriteCmd(EXPANDER_ADDR, 0x02, 0x99);
    i2cReplayInitSequence(EXPANDER_ADDR);
    // i2cProbeExpander(EXPANDER_ADDR);
    // i2cRegisterSweep(EXPANDER_ADDR);
    // i2cCommandProbe(EXPANDER_ADDR);
    // i2cCommandOutputTest(EXPANDER_ADDR);
    // i2cCommandBankScan(EXPANDER_ADDR);
    // i2cCommandHighNibbleScan(EXPANDER_ADDR);
    // i2cCommand3ByteScan(EXPANDER_ADDR);
    // i2cUnlockSequenceTest(EXPANDER_ADDR);
    // i2cSoftResetRecover(EXPANDER_ADDR);
  }

#if 0

  Serial.println("\nSystem starting...\n");
  Serial.println("EleksTubeHAX https://github.com/aly-fly/EleksTubeHAX");
  Serial.printf("Firmware version: v%s.\n", FIRMWARE_VERSION);

  // Prepare unique device name
#ifdef MQTT_CLIENT_ID_FOR_SMARTNEST
  snprintf(UniqueDeviceName, sizeof(UniqueDeviceName), "%s", MQTT_CLIENT_ID_FOR_SMARTNEST); // Use fixed ID for smartnest.cz
#else
  {
    // Generate unique ID for other cases
    // ESP.getEfuseMac() returns the 48-bit MAC address in the lower bits of the value (little-endian byte order when viewed as an integer).
    // We extract all 6 bytes and store them in the canonical MAC order (MSB first).
    uint64_t rawmac = ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL; // Keep only lower 48 bits, comes in reverse order
    uint8_t mac_bytes[6];
    for (int i = 0; i < 6; i++)
    {
      mac_bytes[i] = (rawmac >> (8 * i)) & 0xFF; // LSB first
    }
    // Generate a unique device name using the last 3 bytes of the MAC address to make the name shorter but still unique enough to avoid collisions
    snprintf(UniqueDeviceName, sizeof(UniqueDeviceName), "%s-%02X%02X%02X", DEVICE_NAME,
             mac_bytes[3], mac_bytes[4], mac_bytes[5]);
  }
#endif // #ifdef MQTT_CLIENT_ID_FOR_SMARTNEST
  // Prepare lowercase variant for MQTT topic usage
  for (size_t i = 0; i < sizeof(UniqueDeviceName); ++i)
  {
    char c = UniqueDeviceName[i];
    UniqueDeviceName[i] = (char)tolower((int)c);
    if (c == '\0')
      break;
  }
  Serial.printf("Set device name: \"%s\".\n", UniqueDeviceName);

  Serial.print("Init NVS flash partition usage...");
  esp_err_t ret = nvs_flash_init(); // Initialize NVS
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    Serial.println("");
    Serial.println("No free pages in or newer version of NVS partition found. Erasing NVS flash partition...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  Serial.println("Done.");

  stored_config.begin();
  stored_config.load();

  backlights.begin(&stored_config.config.backlights);
  buttons.begin();
  menu.begin();

  // Setup the displays (TFTs) initaly and show bootup message(s).
  tfts.begin(); // ...and count number of clock faces available...
  tfts.fillScreen(TFT_BLACK);
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.setCursor(0, 0, 2); // Font 2. 16 pixel high
  tfts.println("Starting Setup...");

#ifdef HARDWARE_NOVELLIFE_CLOCK
  // Init the Gesture sensor
  tfts.setTextColor(TFT_ORANGE, TFT_BLACK);
  tfts.print("Gest start...");
  Serial.print("Gesture Sensor start...");
  GestureStart(); // TODO put into class
  tfts.println("Done!");
  Serial.println("Done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
#endif // #ifdef HARDWARE_NOVELLIFE_CLOCK

  // Setup WiFi connection. Must be done before setting up Clock.
  // This is done outside Clock so the network can be used for other things.
  tfts.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tfts.println("WiFi start...");
  Serial.println("WiFi start...");
  WifiBegin();
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

  // Wait a bit (5x100ms = 0.5 sec) before querying NTP.
  for (uint8_t ndx = 0; ndx < 5; ndx++)
  {
    tfts.print(">");
    delay(100);
  }
  tfts.println("");

  // Setup the clock. It needs WiFi to be established already.
  tfts.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tfts.print("Clock start...");
  Serial.println("\nClock start-up...");
  uclock.begin(&stored_config.config.uclock);
  tfts.println("Done!");
  Serial.println("\nClock start-up done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
  // Setup MQTT.
  tfts.setTextColor(TFT_YELLOW, TFT_BLACK);
  tfts.print("MQTT start...");
  Serial.println("\nMQTT start...");
  MQTTStart(false);
  tfts.println("Done!");
  Serial.println("MQTT start Done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
#endif

#ifdef GEOLOCATION_ENABLED
  tfts.setTextColor(TFT_CYAN, TFT_BLACK);
  tfts.println("GeoLoc query...");
  if (GetGeoLocationTimeZoneOffset())
  {
    tfts.print("TZ: ");
    Serial.print("TZ: ");
    tfts.println(GeoLocTZoffset);
    Serial.println(GeoLocTZoffset);
    uclock.setTimeZoneOffset(GeoLocTZoffset * 3600);
    Serial.println();
    Serial.print("Saving config! Triggered by timezone change...");
    stored_config.save();
    tfts.println("Done!");
    Serial.println("Done!");
    tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  else
  {
    tfts.setTextColor(TFT_RED, TFT_BLACK);
    tfts.println("GeoLoc FAILED");
    Serial.println("GeoLoc failed!");
    tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  }
#endif

  if (uclock.getActiveGraphicIdx() > tfts.NumberOfClockFaces)
  {
    uclock.setActiveGraphicIdx(tfts.NumberOfClockFaces);
    Serial.println("Last selected index of clock face is larger than currently available number of image sets.");
  }
  if (uclock.getActiveGraphicIdx() < 1)
  {
    uclock.setActiveGraphicIdx(1);
    Serial.println("Last selected index of clock face is less than 1.");
  }
  tfts.current_graphic = uclock.getActiveGraphicIdx();

  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.println("Done with Setup!");
  Serial.println("\nDone with Setup!");

  // Leave bootup messages on screen for a few seconds (10x200ms = 2 sec).
  for (uint8_t ndx = 0; ndx < 10; ndx++)
  {
    tfts.print(">");
    delay(200);
  }

  // Start up the clock displays.
  tfts.fillScreen(TFT_BLACK);
  uclock.loop();
  updateClockDisplay(TFTs::force); // Draw all the clock digits
  Serial.println("Starting main loop...");
#endif
}

//-----------------------------------------------------------------------
// Main loop
//-----------------------------------------------------------------------
void loop()
{
  static bool tft_inited[NUM_DIGITS] = {false, false, false, false, false, false};
  static uint8_t digit_idx = 0;
  static uint32_t last_tick = 0;
  static bool clear_after_cycle = false;

  const uint8_t cs_masks[NUM_DIGITS] = {0xFE, 0xFD, 0xFB, 0xDF, 0xBF, 0x7F};
  const uint16_t colors[NUM_DIGITS] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA};

  if (expander_present)
  {
    if ((millis() - last_tick) >= 1000)
    {
      last_tick = millis();

      if (clear_after_cycle)
      {
        for (uint8_t i = 0; i < NUM_DIGITS; i++)
        {
          expanderWriteCmd(EXPANDER_ADDR, 0x00, cs_masks[i]);
          delay(2);
          if (!tft_inited[i])
          {
            test_tft.init();
            test_tft.setRotation(0);
            test_tft.invertDisplay(true);
            tft_inited[i] = true;
          }
          test_tft.fillScreen(TFT_BLACK);
        }
        expanderWriteCmd(EXPANDER_ADDR, 0x00, 0xFF);
        clear_after_cycle = false;
      }
      else
      {
        expanderWriteCmd(EXPANDER_ADDR, 0x00, cs_masks[digit_idx]);
        delay(2);

        if (!tft_inited[digit_idx])
        {
          test_tft.init();
          test_tft.setRotation(0);
          test_tft.invertDisplay(true);
          tft_inited[digit_idx] = true;
        }
        test_tft.fillScreen(colors[digit_idx]);

        expanderWriteCmd(EXPANDER_ADDR, 0x00, 0xFF);
        digit_idx = (uint8_t)((digit_idx + 1) % NUM_DIGITS);
        if (digit_idx == 0)
        {
          clear_after_cycle = true;
        }
      }
    }
  }

  delay(5);

#if 0
  uint32_t millis_at_top = millis();

  // Do all the maintenance work.
  WifiReconnect(); // If not connected to WiFi, attempt to reconnect

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
  MQTTLoopFrequently();

  bool MQTTCommandReceived =
      MQTTCommandMainPowerReceived ||
      MQTTCommandBackPowerReceived ||
      MQTTCommandStateReceived ||
      MQTTCommandBrightnessReceived ||
      MQTTCommandMainBrightnessReceived ||
      MQTTCommandBackBrightnessReceived ||
      MQTTCommandPatternReceived ||
      MQTTCommandBackPatternReceived ||
      MQTTCommandBackColorPhaseReceived ||
      MQTTCommandGraphicReceived ||
      MQTTCommandMainGraphicReceived ||
      MQTTCommandUseTwelveHoursReceived ||
      MQTTCommandBlankZeroHoursReceived ||
      MQTTCommandPulseBpmReceived ||
      MQTTCommandBreathBpmReceived ||
      MQTTCommandRainbowSecReceived;

  if (MQTTCommandMainPowerReceived)
  {
    MQTTCommandMainPowerReceived = false;
    if (MQTTCommandMainPower)
    {
      // Perform reinit, enable, redraw only if displays are actually off. HA sends ON command together with clock face change which causes flickering.
      if (!tfts.isEnabled())
      {
#ifdef HARDWARE_ELEKSTUBE_CLOCK // Original EleksTube hardware and direct clones need a reinit to wake up the displays properly
        tfts.reinit();
#else
        tfts.enableAllDisplays(); // For all other clocks, just enable the displays
#endif
        updateClockDisplay(TFTs::force); // Redraw all the clock digits; needed because the displays was blanked before turning off
      }
    }
    else
    {
      tfts.chip_select.setAll();
      tfts.fillScreen(TFT_BLACK); // Blank the screens before turning off; needed for all clocks without a real power switch circuit to "simulate" the switched-off displays
      tfts.disableAllDisplays();
    }
  }

  if (MQTTCommandBackPowerReceived)
  {
    MQTTCommandBackPowerReceived = false;
    if (MQTTCommandBackPower)
    {
      backlights.PowerOn();
    }
    else
    {
      backlights.PowerOff();
    }
  }

  if (MQTTCommandStateReceived)
  {
    MQTTCommandStateReceived = false;
    randomSeed(millis());
    uint8_t idx;
    if (MQTTCommandState >= 90)
    {
      idx = random(1, tfts.NumberOfClockFaces + 1);
    }
    else
    {
      idx = (MQTTCommandState / 5) - 1;
    } // 10..40 -> graphic 1..6
    Serial.print("Graphic change request from MQTT; command: ");
    Serial.print(MQTTCommandState);
    Serial.print(", index: ");
    Serial.println(idx);
    uclock.setClockGraphicsIdx(idx);
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    updateClockDisplay(TFTs::force); // Redraw everything
  }

  if (MQTTCommandMainBrightnessReceived)
  {
    MQTTCommandMainBrightnessReceived = false;
    tfts.dimming = MQTTCommandMainBrightness;
    tfts.ProcessUpdatedDimming();
    updateClockDisplay(TFTs::force);
  }

  if (MQTTCommandBackBrightnessReceived)
  {
    MQTTCommandBackBrightnessReceived = false;
    backlights.setIntensity(uint8_t(MQTTCommandBackBrightness));
  }

  if (MQTTCommandPatternReceived)
  {
    MQTTCommandPatternReceived = false;

    for (int8_t i = 0; i < Backlights::num_patterns; i++)
    {
      Serial.print("New pattern ");
      Serial.print(MQTTCommandPattern);
      Serial.print(", check pattern ");
      Serial.println(Backlights::patterns_str[i]);
      if (strcmp(MQTTCommandPattern, (Backlights::patterns_str[i]).c_str()) == 0)
      {
        backlights.setPattern(Backlights::patterns(i));
        break;
      }
    }
  }

  if (MQTTCommandBackPatternReceived)
  {
    MQTTCommandBackPatternReceived = false;
    for (int8_t i = 0; i < Backlights::num_patterns; i++)
    {
      Serial.print("new pattern ");
      Serial.print(MQTTCommandBackPattern);
      Serial.print(", check pattern ");
      Serial.println(Backlights::patterns_str[i]);
      if (strcmp(MQTTCommandBackPattern, (Backlights::patterns_str[i]).c_str()) == 0)
      {
        backlights.setPattern(Backlights::patterns(i));
        break;
      }
    }
  }

  if (MQTTCommandBackColorPhaseReceived)
  {
    MQTTCommandBackColorPhaseReceived = false;

    backlights.setColorPhase(MQTTCommandBackColorPhase);
  }

  if (MQTTCommandGraphicReceived)
  {
    MQTTCommandGraphicReceived = false;

    uclock.setClockGraphicsIdx(MQTTCommandGraphic);
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    updateClockDisplay(TFTs::force); // Redraw everything
  }

  if (MQTTCommandMainGraphicReceived)
  {
    MQTTCommandMainGraphicReceived = false;
    uclock.setClockGraphicsIdx(MQTTCommandMainGraphic);
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    updateClockDisplay(TFTs::force); // Redraw everything
  }

  if (MQTTCommandUseTwelveHoursReceived)
  {
    MQTTCommandUseTwelveHoursReceived = false;
    uclock.setTwelveHour(MQTTCommandUseTwelveHours);
  }

  if (MQTTCommandBlankZeroHoursReceived)
  {
    MQTTCommandBlankZeroHoursReceived = false;
    uclock.setBlankHoursZero(MQTTCommandBlankZeroHours);
  }

  if (MQTTCommandPulseBpmReceived)
  {
    MQTTCommandPulseBpmReceived = false;
    backlights.setPulseRate(MQTTCommandPulseBpm);
  }

  if (MQTTCommandBreathBpmReceived)
  {
    MQTTCommandBreathBpmReceived = false;
    backlights.setBreathRate(MQTTCommandBreathBpm);
  }

  if (MQTTCommandRainbowSecReceived)
  {
    MQTTCommandRainbowSecReceived = false;
    backlights.setRainbowDuration(MQTTCommandRainbowSec);
  }

  MQTTStatusMainPower = tfts.isEnabled();
  MQTTStatusBackPower = backlights.getPower();
  MQTTStatusState = (uclock.getActiveGraphicIdx() + 1) * 5; // 10
  MQTTStatusBrightness = backlights.getIntensity();
  MQTTStatusMainBrightness = tfts.dimming;
  MQTTStatusBackBrightness = backlights.getIntensity();
  strcpy(MQTTStatusPattern, backlights.getPatternStr().c_str());
  strcpy(MQTTStatusBackPattern, backlights.getPatternStr().c_str());
  backlights.getPatternStr().toCharArray(MQTTStatusBackPattern, backlights.getPatternStr().length() + 1);
  MQTTStatusBackColorPhase = backlights.getColorPhase();
  MQTTStatusGraphic = uclock.getActiveGraphicIdx();
  MQTTStatusMainGraphic = uclock.getActiveGraphicIdx();
  MQTTStatusUseTwelveHours = uclock.getTwelveHour();
  MQTTStatusBlankZeroHours = uclock.getBlankHoursZero();
  MQTTStatusPulseBpm = backlights.getPulseRate();
  MQTTStatusBreathBpm = backlights.getBreathRate();
  MQTTStatusRainbowSec = backlights.getRainbowDuration();

  if (MQTTCommandReceived)
  {
    lastMQTTCommandExecuted = millis();

    MQTTReportBackEverything(true);
  }

  if (lastMQTTCommandExecuted != -1)
  {
    if (((millis() - lastMQTTCommandExecuted) > (MQTT_SAVE_PREFERENCES_AFTER_SEC * 1000)) && menu.getState() == Menu::idle)
    { // Save the config after a while (default is 60 seconds) if no new MQTT command was received and we are not in the menu.
      lastMQTTCommandExecuted = -1;

      Serial.print("Saving config...");
      stored_config.save();
      Serial.println(" Done.");
    }
  }
#endif

  buttons.loop();

#ifdef HARDWARE_NOVELLIFE_CLOCK
  HandleGestureInterupt();
#endif // #ifdef HARDWARE_NOVELLIFE_CLOCK

// if the device has one button only, no power button functionality is needed!
#ifndef ONE_BUTTON_ONLY_MENU
  // Power button: If in menu, exit menu. Else turn off displays and backlight.
  if (buttons.power.isDownEdge() && (menu.getState() == Menu::idle))
  { // Power button was pressed: if in the menu, exit menu, else turn off displays and backlight.
    if (tfts.isEnabled())
    { // Check if TFT state is enabled and switch OFF the LCDs and LED backlights.
      tfts.chip_select.setAll();
      tfts.fillScreen(TFT_BLACK); // Blank the screens before turning off; needed for all clocks without a real power switch circuit
      tfts.disableAllDisplays();
      backlights.PowerOff();
    }
    else
    {                           // TFT state is disabled, turn ON the displays and backlights
#ifdef HARDWARE_ELEKSTUBE_CLOCK // Original EleksTube hardware and direct clones need a reinit to wake up the displays properly
      tfts.reinit();
#else
      tfts.enableAllDisplays(); // For all other clocks, just enable the displays
#endif
      updateClockDisplay(TFTs::force); // Redraw all the clock digits; needed because the displays was blanked before turning off
      backlights.PowerOn();
    }
  }
#endif // ONE_BUTTON_ONLY_MENU

  menu.loop(buttons); // Must be called after buttons.loop()
  backlights.loop();
  uclock.loop();

#ifdef DIMMING
  checkDimmingNeeded(); // Night or day time brightness change
#endif

  updateClockDisplay(); // Draw only the changed clock digits!

#ifdef GEOLOCATION_ENABLED
  checkUpdateGeoLocNeeded(); // Check if it is time to update geolocation based timezone offset (just once per day)
#endif

  // Menu
  if (menu.stateChanged() && tfts.isEnabled())
  {
    Menu::states menu_state = menu.getState();
    int8_t menu_change = menu.getChange();

    if (menu_state == Menu::idle)
    {
      // We just changed into idle, so force a redraw of all clock digits and save the config.
      updateClockDisplay(TFTs::force); // Redraw everything
      Serial.println();
      Serial.print("Saving config! Triggered from leaving menu...");
      stored_config.save();
      Serial.println(" Done.");
    }
    else
    {
      // Backlight Pattern
      if (menu_state == Menu::backlight_pattern)
      {
        if (menu_change != 0)
        {
          backlights.setNextPattern(menu_change);
        }
        setupMenu();
        tfts.println("Pattern:");
        tfts.println(backlights.getPatternStr());
      }
      // Backlight Color
      else if (menu_state == Menu::pattern_color)
      {
        if (menu_change != 0)
        {
          backlights.adjustColorPhase(menu_change * 16);
        }
        setupMenu();
        tfts.println("Color:");
        tfts.printf("%06X\n", backlights.getColor());
      }
      // Backlight Intensity
      else if (menu_state == Menu::backlight_intensity)
      {
        if (menu_change != 0)
        {
          backlights.adjustIntensity(menu_change);
        }
        setupMenu();
        tfts.println("Intensity:");
        tfts.println(backlights.getIntensity());
      }
      // 12 Hour or 24 Hour mode?
      else if (menu_state == Menu::twelve_hour)
      {
        if (menu_change != 0)
        {
          uclock.toggleTwelveHour();
          tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), TFTs::force);
          tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), TFTs::force);
        }
        setupMenu();
        tfts.println("Hour format");
        tfts.println(uclock.getTwelveHour() ? "12 hour" : "24 hour");
      }
      // Blank leading zeros on the hours?
      else if (menu_state == Menu::blank_hours_zero)
      {
        if (menu_change != 0)
        {
          uclock.toggleBlankHoursZero();
          tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), TFTs::force);
        }
        setupMenu();
        tfts.println("Blank zero?");
        tfts.println(uclock.getBlankHoursZero() ? "yes" : "no");
      }
      // UTC Offset, hours
      else if (menu_state == Menu::utc_offset_hour)
      {
        time_t currOffset = uclock.getTimeZoneOffset();

        if (menu_change != 0)
        {
          // calculate the new offset
          time_t newOffsetAdjustmentValue = menu_change * 3600;
          time_t newOffset = currOffset + newOffsetAdjustmentValue;

          // check if the new offset is within the allowed range of -12 to +12 hours
          // If the minutes part of the offset is 0, we want to change from +12 to -12 or vice versa (without changing the shown time on the displays)
          // If the minutes part is not 0: We want to wrap around to the other side and change the minutes part (i.e. from 11:45 directly to -11:15)
          bool offsetWrapAround = false;
          if (newOffset > 43200)
          { // we just "passed" +12 hours -> set to -12 hours
            newOffset = -43200;
            offsetWrapAround = true;
          }
          if (newOffset < -43200 && !offsetWrapAround)
          { // we just passed -12 hours -> set to +12 hours
            newOffset = 43200;
          }

          uclock.setTimeZoneOffset(newOffset); // set the new offset
          uclock.loop();                       // update the clock time and redraw the changed digits -> will "flicker" the menu for a short time, but without, menu is not redrawn correctly
#ifdef DIMMING
          checkDimmingNeeded(); // check if we need dimming for the night, because timezone was changed
#endif
          currOffset = uclock.getTimeZoneOffset(); // get the new offset as current offset for the menu
        }
        setupMenu();
        tfts.println("UTC Offset");
        tfts.println(" +/- Hour");
        char offsetStr[11];
        int8_t offset_hour = currOffset / 3600;
        int8_t offset_min = (currOffset % 3600) / 60;
        if (offset_min <= 0 && offset_hour <= 0)
        { // negative timezone value -> Make them positive and print a minus in front
          offset_min = -offset_min;
          offset_hour = -offset_hour;
          snprintf(offsetStr, sizeof(offsetStr), "-%d:%02d", offset_hour, offset_min);
        }
        else
        {
          if (offset_min >= 0 && offset_hour >= 0)
          { // postive timezone value for hours and minutes -> show a plus in front
            snprintf(offsetStr, sizeof(offsetStr), "+%d:%02d", offset_hour, offset_min);
          }
        }
        if (offset_min == 0 && offset_hour == 0)
        { // we don't want a sign in front of the 0:00 case
          snprintf(offsetStr, sizeof(offsetStr), "%d:%02d", offset_hour, offset_min);
        }
        tfts.println(offsetStr);
      } // END UTC Offset, hours
      // BEGIN UTC Offset, 15 minutes
      else if (menu_state == Menu::utc_offset_15m)
      {
        time_t currOffset = uclock.getTimeZoneOffset();

        if (menu_change != 0)
        {
          time_t newOffsetAdjustmentValue = menu_change * 900; // calculate the new offset
          time_t newOffset = currOffset + newOffsetAdjustmentValue;

          // check if the new offset is within the allowed range of -12 to +12 hours
          // same behaviour as for the +/-1 hour offset, but with 15 minutes steps
          bool offsetWrapAround = false;
          if (newOffset > 43200)
          { // we just "passed" +12 hours -> set to -12 hours
            newOffset = -43200;
            offsetWrapAround = true;
          }
          if (newOffset < -43200 && !offsetWrapAround)
          { // we just passed -12 hours -> set to +12 hours
            newOffset = 43200;
          }

          uclock.setTimeZoneOffset(newOffset); // set the new offset
          uclock.loop();                       // update the clock time and redraw the changed digits -> will "flicker" the menu for a short time, but without, menu is not redrawn correctly
#ifdef DIMMING
          checkDimmingNeeded(); // check if we need dimming for the night, because timezone was changed
#endif
          currOffset = uclock.getTimeZoneOffset(); // get the new offset as current offset for the menu
        }
        setupMenu();
        tfts.println("UTC Offset");
        tfts.println(" +/- 15m");
        char offsetStr[11];
        int8_t offset_hour = currOffset / 3600;
        int8_t offset_min = (currOffset % 3600) / 60;
        if (offset_min <= 0 && offset_hour <= 0)
        { // negative timezone value -> Make them positive and print a minus in front
          offset_min = -offset_min;
          offset_hour = -offset_hour;
          snprintf(offsetStr, sizeof(offsetStr), "-%d:%02d", offset_hour, offset_min);
        }
        else
        {
          if (offset_min >= 0 && offset_hour >= 0)
          { // postive timezone value for hours and minutes -> show a plus in front
            snprintf(offsetStr, sizeof(offsetStr), "+%d:%02d", offset_hour, offset_min);
          }
        }
        if (offset_min == 0 && offset_hour == 0)
        { // we don't want a sign in front of the 0:00 case so overwrite the string
          snprintf(offsetStr, sizeof(offsetStr), "%d:%02d", offset_hour, offset_min);
        }
        tfts.println(offsetStr);
      } // END UTC Offset, 15 minutes
      // select clock face
      else if (menu_state == Menu::selected_graphic)
      {
        if (menu_change != 0)
        {
          uclock.adjustClockGraphicsIdx(menu_change);

          if (tfts.current_graphic != uclock.getActiveGraphicIdx())
          {
            tfts.current_graphic = uclock.getActiveGraphicIdx();
            updateClockDisplay(TFTs::force); // Redraw everything
          }
        }
        setupMenu();
        tfts.println("Selected");
        tfts.println("graphic:");
        tfts.printf("    %d\n", uclock.getActiveGraphicIdx());
      }
#ifdef WIFI_USE_WPS //  WPS code
      // connect to WiFi using wps pushbutton mode
      else if (menu_state == Menu::start_wps)
      {
        if (menu_change != 0)
        { // button was pressed
          if (menu_change < 0)
          { // left button
            Serial.println("WiFi WPS start request");
            tfts.clear();
            tfts.fillScreen(TFT_BLACK);
            tfts.setTextColor(TFT_WHITE, TFT_BLACK);
            tfts.setCursor(0, 0, 4); // Font 4. 26 pixel high
            WiFiStartWps();
          }
        }
        setupMenu();
        tfts.println("Connect to WiFi?");
        tfts.println("Left=WPS");
      }
#endif
    }
  } // if (menu.stateChanged())

  uint32_t time_in_loop = millis() - millis_at_top;
  if (time_in_loop < 20)
  {
    // we have free time (loop run took under 20 ms), spend it for loading next image into buffer
    tfts.LoadNextImage();

    // Do we still have extra time? -> normally not in the same loop where image loading was done, but in the next loops
    time_in_loop = millis() - millis_at_top;
    if (time_in_loop < 20)
    {
#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
      MQTTLoopInFreeTime(); // do less time critical MQTT tasks
#endif

#ifdef GEOLOCATION_ENABLED
      processGeoLocUpdate();
#endif // GEOLOCATION_ENABLED
      // Sleep for up to 20ms, less if we've spent time doing stuff above.
      time_in_loop = millis() - millis_at_top;
      if (time_in_loop < 20) // loop was faster than 20ms -> unusually fast, yield some time to other tasks
      {
        delay(20 - time_in_loop);
      }
    }
  }
#ifdef DEBUG_OUTPUT
  if (time_in_loop <= 2) // if the loop time is less than 2ms, we don't need to print it in detail
    Serial.print(".");
  else
  {
    Serial.print("time spent in loop (ms): "); // print the time spent in the loop
    Serial.println(time_in_loop);
  }
#endif // DEBUG_OUTPUT
#endif
}

void setupMenu()
{                                  // Prepare drawing of the menu texts
  tfts.chip_select.setHoursTens(); // use most left display
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.fillRect(0, 120, 135, 120, TFT_BLACK); // use lower half of the display, fill with black
  tfts.setCursor(0, 124, 4);                  // use font 4 - 26 pixel high - for the menu text
}

#ifdef DIMMING
bool isNightTime(uint8_t current_hour)
{ // check the actual hour is in the defined "night time"
  if (DAY_TIME < NIGHT_TIME)
  { // "Night" spans across midnight so it is split between two days
    return (current_hour < DAY_TIME) || (current_hour >= NIGHT_TIME);
  }
  else
  { // "Night" starts after midnight, entirely contained within the current day
    return (current_hour >= NIGHT_TIME) && (current_hour < DAY_TIME);
  }
}

void checkDimmingNeeded()
{                                             // dim the display in the defined night time
  uint8_t current_hour = uclock.getHour24();  // for internal calcs we always use 24h format
  isDimmingNeeded = current_hour != hour_old; // check, if the hour has changed since last loop (from time passing by or from timezone change)
  if (isDimmingNeeded)
  {
    Serial.print("Current hour = ");
    Serial.print(current_hour);
    Serial.print(", Night Time Start = ");
    Serial.print(NIGHT_TIME);
    Serial.print(", Day Time Start = ");
    Serial.println(DAY_TIME);
    if (isNightTime(current_hour))
    { // check if it is in the defined night time
      Serial.println("Set to night time mode (dimmed)!");
      tfts.dimming = TFT_DIMMED_INTENSITY;
      tfts.ProcessUpdatedDimming();
      backlights.setDimming(true);
    }
    else
    {
      Serial.println("Set to day time mode (max brightness)!");
      tfts.dimming = 255; // 0..255
      tfts.ProcessUpdatedDimming();
      backlights.setDimming(false);
    }
    updateClockDisplay(TFTs::force); // Redraw everything; software dimming will be done here
    hour_old = current_hour;
  }
}
#endif // DIMMING

#ifdef GEOLOCATION_ENABLED
bool GetGeoLocationTimeZoneOffset()
{
  Serial.println("\nStarting Geolocation API query...");

#ifdef GEOLOCATION_PROVIDER_IPAPI
  // Use IP-API.com -> Free tier has a 45 requests per minute limit!
  IPGeolocation location(GEOLOCATION_API_KEY, "IPAPI");
#elif defined(GEOLOCATION_PROVIDER_IPGEOLOCATION)
  // Use ipgeolocation.io -> Free tier has 1,000 requests per month limit!
  IPGeolocation location(GEOLOCATION_API_KEY, "IPGEOLOCATION");
#elif defined(GEOLOCATION_PROVIDER_ABSTRACTAPI)
  // Use AbstractAPI.com -> Free tier has 1,000 requests AT ALL per account!
  IPGeolocation location(GEOLOCATION_API_KEY, "ABSTRACTAPI");
#else
  // No provider defined -> default to IP-API.com
  IPGeolocation location(GEOLOCATION_API_KEY, "IPAPI");
#endif

  IPGeo ipg;
  if (location.updateStatus(&ipg))
  {
    Serial.println(String("Geo Time Zone: ") + String(ipg.tz));
    Serial.println(String("Geo TZ Offset: ") + String(ipg.offset));          // primary value of interest
    Serial.println(String("Geo Current Time: ") + String(ipg.current_time)); // currently unused but handy for debugging
    const double rawOffsetHours = ipg.offset;
    const int32_t newOffsetSeconds = static_cast<int32_t>(lround(rawOffsetHours * 3600.0));

    if ((newOffsetSeconds % (15 * 60)) != 0)
    {
      Serial.print("GeoLoc rejected offset not aligned to 15 min grid (seconds): ");
      Serial.println(newOffsetSeconds);
      return false;
    }

    const bool hasValidStoredOffset = stored_config.config.uclock.is_valid == StoredConfig::valid;
    const int32_t previousOffsetSeconds = static_cast<int32_t>(stored_config.config.uclock.time_zone_offset);
    const int32_t defaultOffsetSeconds = 1 * 3600;

    if (hasValidStoredOffset && previousOffsetSeconds != 0 && previousOffsetSeconds != defaultOffsetSeconds)
    {
      int32_t diff = newOffsetSeconds - previousOffsetSeconds;
      if (diff < 0)
      {
        diff = -diff;
      }

      if (diff > (2 * 3600)) // more than 2 hours difference -> reject
      {
        Serial.print("GeoLoc offset deviates by more than 2h from stored value (prev: ");
        Serial.print(previousOffsetSeconds);
        Serial.print("s, new: ");
        Serial.print(newOffsetSeconds);
        Serial.println("s). Ignoring update.");
        return false;
      }
    }

    GeoLocTZoffset = static_cast<double>(newOffsetSeconds) / 3600.0;
    Serial.println(String("Geo TZ Offset (applied): ") + String(GeoLocTZoffset));
    return true;
  }

  Serial.println("Geolocation failed.");
  return false;
}
#endif

#ifdef GEOLOCATION_ENABLED
void checkUpdateGeoLocNeeded()
{
  uint8_t currentDay = uclock.getDay(); // Get current day of month
  const uint8_t currentWeekday = weekday(uclock.loop_time);
  const bool isSunday = (currentWeekday == 1); // TimeLib defines Sunday as weekday 1

  // Only on Sundays, and only if the day has changed since last successful update
  // `isGeoLocWindow` is set to true between 03:00:05 and 03:00:59, because daylight saving time changes usually happen at 03:00 local time.
  // GeoLoc update mechanism in the main 'loop free time slot' is then triggered, because `GeoLocNeedsUpdate` is set to true here.
  const bool isGeoLocWindow = isSunday && (currentDay != yesterday) && (uclock.getHour24() == 3) && (uclock.getMinute() == 0) && (uclock.getSecond() > 5);

  if (!GeoLocNeedsUpdate && isGeoLocWindow)
  {
    Serial.print("GeoLoc needs update! Current date (DD.MM.YYYY): ");
    Serial.print(currentDay);
    Serial.print(".");
    Serial.print(uclock.getMonth());
    Serial.print(".");
    Serial.println(uclock.getYear());

    // Set flags and counters for GeoLoc update process
    GeoLocNeedsUpdate = true;
    GeoLocFailedAttempts = 0;
    GeoLocNextRetryMillis = 0;
    GeoLocAttemptDay = currentDay;
  }
}

void processGeoLocUpdate()
{
  if (!GeoLocNeedsUpdate)
  {
    return; // no update needed
  }

  const uint32_t now = millis();
  const uint8_t today = uclock.getDay();

  if (GeoLocAttemptDay != today)
  { // New day, reset attempt counter
    GeoLocFailedAttempts = 0;
    GeoLocNextRetryMillis = 0;
    GeoLocAttemptDay = today;
  }

  if (GeoLocFailedAttempts >= GEOLOC_MAX_FAILURES_PER_DAY)
  {
    Serial.println("GeoLocation update skipped: failure limit reached for today.");
    GeoLocNeedsUpdate = false;
    return; // give up for today
  }

  if (now < GeoLocNextRetryMillis)
  {
    return; // not yet time for next retry
  }

  Serial.println("Daily update for geolocation timezone offset...");

  const int32_t GeoLocTZOffsetOld = uclock.getTimeZoneOffset() / 3600;
  Serial.print("Current TZ offset (hours): ");
  Serial.println(GeoLocTZOffsetOld);

  Serial.println("Querying GeoLocation API...");
  if (GetGeoLocationTimeZoneOffset())
  {
    const int32_t GeoLocTOffsetNew = uclock.getTimeZoneOffset() / 3600;
    Serial.print("New TZ offset (hours): ");
    Serial.println(GeoLocTOffsetNew);

    GeoLocNeedsUpdate = false;
    GeoLocFailedAttempts = 0;
    GeoLocNextRetryMillis = 0;
    yesterday = today;
    return; // success for today!
  }

  GeoLocFailedAttempts++;
  if (GeoLocFailedAttempts >= GEOLOC_MAX_FAILURES_PER_DAY)
  {
    Serial.println("GeoLocation update aborted after repeated failures today.");
    GeoLocNeedsUpdate = false;
    return; // give up for today
  }

  // Schedule next retry
  GeoLocNextRetryMillis = now + GEOLOC_RETRY_BACKOFF_MS;
  Serial.print("GeoLocation retry scheduled in ");
  Serial.print(GEOLOC_RETRY_BACKOFF_MS / 1000);
  Serial.println(" seconds.");
}
#endif // GEOLOCATION_ENABLED

void updateClockDisplay(TFTs::show_t show)
{
  // Refresh, starting with seconds.
  tfts.setDigit(SECONDS_ONES, uclock.getSecondsOnes(), show);
  tfts.setDigit(SECONDS_TENS, uclock.getSecondsTens(), show);
  tfts.setDigit(MINUTES_ONES, uclock.getMinutesOnes(), show);
  tfts.setDigit(MINUTES_TENS, uclock.getMinutesTens(), show);
  tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), show);
  tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), show);
}
