#include <Arduino.h>
#include "MQTT_client_ips.h"
#include "TFTs.h"
#include "WiFi_WPS.h"
#include "esp_err.h"
#include "esp_spiffs.h"

#ifdef DEBUG_TFT_GUARD
#define TFT_GUARD_LOG(fmt, ...) Serial.printf("[TFT GUARD] " fmt, ##__VA_ARGS__)
#else
#define TFT_GUARD_LOG(fmt, ...) ((void)0)
#endif

namespace
{
constexpr uint16_t kMaxBmpPaletteEntries = 256;

inline bool ensureRange(int32_t value, int32_t minValue, int32_t maxValue)
{
  return (value >= minValue) && (value <= maxValue);
}
} // namespace

void TFTs::begin()
{
  chip_select.begin();
  chip_select.setAll(); // Start with all displays selected

#ifdef DIM_WITH_ENABLE_PIN_PWM
  // If hardware dimming is used, initialise the LEDC output for the enable pin
  ledcSetup(TFT_PWM_CHANNEL, TFT_PWM_FREQ, TFT_PWM_RESOLUTION);
  ledcAttachPin(TFT_ENABLE_PIN, TFT_PWM_CHANNEL);
  ledcChangeFrequency(TFT_PWM_CHANNEL, TFT_PWM_FREQ, TFT_PWM_RESOLUTION);
#else
  pinMode(TFT_ENABLE_PIN, OUTPUT); // Set pin for turning display power on and off.
#endif
  InvalidateImageInBuffer(); // Signal, that the image in the buffer is invalid and needs to be reloaded and refilled
  init();                    // Initialize the super class.
  fillScreen(TFT_BLACK);     // to avoid/reduce flickering patterns on the screens
  enableAllDisplays();       // Signal, that the displays are enabled now and do the hardware dimming, if available and enabled

  if (!SPIFFS.begin()) // Initialize SPIFFS
  {
    Serial.println("SPIFFS initialization failed!");
    NumberOfClockFaces = 0;
    return;
  }

  // Log some useful information about flash and SPIFFS status
  const FlashMode_t flashMode = ESP.getFlashChipMode();
  const uint32_t flashSpeedHz = ESP.getFlashChipSpeed();

  const char *modeString = "UNKNOWN";
  switch (flashMode)
  {
  case FM_QIO:
    modeString = "QIO";
    break;
  case FM_QOUT:
    modeString = "QOUT";
    break;
  case FM_DIO:
    modeString = "DIO";
    break;
  case FM_DOUT:
    modeString = "DOUT";
    break;
  case FM_FAST_READ:
    modeString = "FAST_READ";
    break;
  case FM_SLOW_READ:
    modeString = "SLOW_READ";
    break;
  default:
    break;
  }

  Serial.printf("[FLASH] mode=%s (%d), speed=%lu Hz\n",
                modeString,
                static_cast<int>(flashMode),
                static_cast<unsigned long>(flashSpeedHz));

  // Log SPIFFS usage information
  size_t spiffsTotal = 0;
  size_t spiffsUsed = 0;
  esp_err_t spiffsStatus = esp_spiffs_info(nullptr, &spiffsTotal, &spiffsUsed);
  if (spiffsStatus == ESP_OK)
  {
    size_t spiffsFree = (spiffsTotal >= spiffsUsed) ? (spiffsTotal - spiffsUsed) : 0;
    const double usedPercent = (spiffsTotal > 0) ? (static_cast<double>(spiffsUsed) * 100.0 / static_cast<double>(spiffsTotal)) : 0.0;
    Serial.printf("[SPIFFS] total=%u bytes, used=%u bytes, free=%u bytes (%.2f%% used)\n",
                  static_cast<unsigned int>(spiffsTotal),
                  static_cast<unsigned int>(spiffsUsed),
                  static_cast<unsigned int>(spiffsFree),
                  usedPercent);
  }
  else
  {
    Serial.printf("[SPIFFS] esp_spiffs_info failed: %s (%d)\n",
                  esp_err_to_name(spiffsStatus),
                  static_cast<int>(spiffsStatus));
  }

  NumberOfClockFaces = CountNumberOfClockFaces();
  loadClockFacesNames();
}

void TFTs::reinit()
{
  if (!TFTsEnabled) // perform re-init only if displays are actually off. HA sends ON command together with clock face change which causes flickering.
  {
#ifndef TFT_SKIP_REINIT
    // Start with all displays selected.
    chip_select.begin();
    chip_select.setAll(); // Start again with all displays selected.

#ifdef DIM_WITH_ENABLE_PIN_PWM
    ledcAttachPin(TFT_ENABLE_PIN, TFT_PWM_CHANNEL);
    ledcChangeFrequency(TFT_PWM_CHANNEL, TFT_PWM_FREQ, TFT_PWM_RESOLUTION);
#else
    pinMode(TFT_ENABLE_PIN, OUTPUT); // Set pin for turning display power on and off.
#endif
    InvalidateImageInBuffer(); // Signal, that the image in the buffer is invalid and needs to be reloaded and refilled
    init();                    // Initialize the super class (again).
    fillScreen(TFT_BLACK);     // to avoid/reduce flickering patterns on the screens
    enableAllDisplays();       // Signal, that the displays are enabled now
#else                          // TFT_SKIP_REINIT
    enableAllDisplays(); // skip full inintialization, just reenable displays by signaling to enable them
#endif                         // TFT_SKIP_REINIT
  }
}

void TFTs::clear()
{
  // Start with all displays selected.
  chip_select.setAll();
  enableAllDisplays();
}

void TFTs::loadClockFacesNames()
{
  int8_t i = 0;
  const char *filename = "/clockfaces.txt";
  Serial.println("Loading clock face names...");
  fs::File f = SPIFFS.open(filename);
  if (!f)
  {
    Serial.println("ERROR: SPIFFS clockfaces.txt not found.");
    return;
  }
  while (f.available() && i < 9)
  {
    patterns_str[i] = f.readStringUntil('\n');
    patterns_str[i].replace("\r", "");
    Serial.println(patterns_str[i]);
    i++;
  }
  f.close();
}

void TFTs::showNoWifiStatus()
{
  chip_select.setSecondsOnes();
  setTextColor(TFT_RED, TFT_BLACK);
  fillRect(0, TFT_HEIGHT - 27, TFT_WIDTH, 27, TFT_BLACK);
  setCursor(5, TFT_HEIGHT - 27, 4); // Font 4. 26 pixel high
  print("NO WiFi!");
}

void TFTs::showNoMqttStatus()
{
  chip_select.setSecondsTens();
  setTextColor(TFT_RED, TFT_BLACK);
  fillRect(0, TFT_HEIGHT - 27, TFT_WIDTH, 27, TFT_BLACK);
  setCursor(5, TFT_HEIGHT - 27, 4);
  print("NO MQTT!");
}

void TFTs::enableAllDisplays()
{
  // Turn "power" on to displays.
  TFTsEnabled = true;
#ifndef DIM_WITH_ENABLE_PIN_PWM
  digitalWrite(TFT_ENABLE_PIN, ACTIVATEDISPLAYS);
#else
  // if hardware dimming is used, only activate with the current dimming value
  ProcessUpdatedDimming();
#endif
}

void TFTs::disableAllDisplays()
{
  // Turn "power" off to displays.
  TFTsEnabled = false;
#ifndef DIM_WITH_ENABLE_PIN_PWM
  digitalWrite(TFT_ENABLE_PIN, DEACTIVATEDISPLAYS);
#else
  // if hardware dimming is used, deactivate via the dimming value
  ProcessUpdatedDimming();
#endif
}

void TFTs::toggleAllDisplays()
{
  if (TFTsEnabled)
  {
    disableAllDisplays();
  }
  else
  {
    enableAllDisplays();
  }
}

void TFTs::setDigit(uint8_t digit, uint8_t value, show_t show)
{
  if (TFTsEnabled)
  { // only do this, if the displays are enabled
    uint8_t old_value = digits[digit];
    digits[digit] = value;

    if (show != no && (old_value != value || show == force))
    {
      showDigit(digit);

      if (digit == SECONDS_ONES)
        if (WifiState != connected)
        {
          showNoWifiStatus();
        }

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
      if (digit == SECONDS_TENS)
        if (!MQTTConnected)
        {
          showNoMqttStatus();
        }
#endif
    }
  }
}

/*
 * Displays the bitmap for the value to the given digit.
 */

void TFTs::showDigit(uint8_t digit)
{
  if (TFTsEnabled)
  { // only do this, if the displays are enabled
    chip_select.setDigit(digit);

    if (digits[digit] == blanked)
    { // Blank Zero
      fillScreen(TFT_BLACK);
    }
    else
    {
      uint8_t file_index = current_graphic * 10 + digits[digit];
      DrawImage(file_index);

      uint8_t NextNumber = digits[SECONDS_ONES] + 1;
      if (NextNumber > 9)
        NextNumber = 0; // pre-load only seconds, because they are drawn first
      NextFileRequired = current_graphic * 10 + NextNumber;
    }
#ifdef HARDWARE_IPSTUBE_CLOCK
    chip_select.update();
#endif
  }
  // else { } //display is disabled, do nothing
}

void TFTs::LoadNextImage()
{
  if (NextFileRequired != FileInBuffer)
  {
#ifdef DEBUG_OUTPUT_IMAGES
    Serial.println("Preload next img");
#endif
    if (!LoadImageIntoBuffer(NextFileRequired))
    {
      TFT_GUARD_LOG("Preload failed for file %u\n", static_cast<unsigned int>(NextFileRequired));
    }
  }
}

void TFTs::InvalidateImageInBuffer()
{                     // force reload from Flash with new dimming settings
  FileInBuffer = 255; // invalid, always load first image
}

void TFTs::ProcessUpdatedDimming()
{
#ifdef DIM_WITH_ENABLE_PIN_PWM
  // hardware dimming is done via PWM on the pin defined by TFT_ENABLE_PIN
  // ONLY for IPSTUBE clocks in the moment! Other clocks may be damaged!
  if (TFTsEnabled)
  {
    ledcWrite(TFT_PWM_CHANNEL, CALCDIMVALUE(dimming));
  }
  else
  {
    // no dimming means 255 (full brightness)
    ledcWrite(TFT_PWM_CHANNEL, CALCDIMVALUE(0));
  }
#else
  // "software" dimming is done via alpha blending in the image drawing function
  // signal that the image in the buffer is invalid and needs to be reloaded and refilled
  InvalidateImageInBuffer();
#endif
}

bool TFTs::FileExists(const char *path)
{
  fs::File f = SPIFFS.open(path, "r");
  bool Exists = ((f == true) && !f.isDirectory());
  f.close();
  return Exists;
}

// These BMP functions are stolen directly from the TFT_SPIFFS_BMP example in the TFT_eSPI library.
// Unfortunately, they aren't part of the library itself, so I had to copy them.
// I've modified DrawImage to buffer the whole image at once instead of doing it line-by-line.

// Too big to fit on the stack.
uint16_t TFTs::UnpackedImageBuffer[TFT_HEIGHT][TFT_WIDTH];

#ifndef USE_CLK_FILES

int8_t TFTs::CountNumberOfClockFaces()
{
  int8_t i, found;
  char filename[10];

  Serial.print("Searching for BMP clock files... ");
  found = 0;
  for (i = 1; i < 10; i++)
  {
    sprintf(filename, "/%d.bmp", i * 10); // search for files 10.bmp, 20.bmp,...
    if (!FileExists(filename))
    {
      found = i - 1;
      break;
    }
  }
  Serial.print(found);
  Serial.println(" fonts found.");
  return found;
}

bool TFTs::LoadImageIntoBuffer(uint8_t file_index)
{
  uint32_t StartTime = millis();

  fs::File bmpFS;
  // Filenames are no bigger than "255.bmp\0"
  char filename[10];
  snprintf(filename, sizeof(filename), "/%u.bmp", static_cast<unsigned int>(file_index));

#ifdef DEBUG_OUTPUT_IMAGES
  Serial.print("Loading: ");
  Serial.println(filename);
#endif

  // Open requested file on SD card
  bmpFS = SPIFFS.open(filename, "r");
  if (!bmpFS)
  {
    Serial.print("File not found: ");
    Serial.println(filename);
    TFT_GUARD_LOG("SPIFFS open failed for %s\n", filename);
    return false;
  }

  bool loadOk = true;

  uint32_t seekOffset = 0;
  uint32_t headerSize = 0;
  uint32_t paletteSize = 0;
  int32_t w = 0;
  int32_t h = 0;
  int32_t row = 0;
  int32_t col = 0;
  uint16_t bitDepth = 0;

  // black background - clear whole buffer
  memset(UnpackedImageBuffer, '\0', sizeof(UnpackedImageBuffer));

  uint16_t magic = read16(bmpFS);
  if (magic == 0xFFFF)
  {
    Serial.print("Can't openfile. Make sure you upload the SPIFFs image with BMPs. : ");
    Serial.println(filename);
    TFT_GUARD_LOG("BMP magic read failed for %s\n", filename);
    loadOk = false;
  }
  else if (magic != 0x4D42)
  {
    Serial.print("File not a BMP. Magic: ");
    Serial.println(magic);
    TFT_GUARD_LOG("Unexpected BMP magic 0x%04X in %s\n", magic, filename);
    loadOk = false;
  }

  if (loadOk)
  {
    read32(bmpFS);              // filesize in bytes (unused)
    read32(bmpFS);              // reserved (unused)
    seekOffset = read32(bmpFS); // start of bitmap
    headerSize = read32(bmpFS); // header size
    w = static_cast<int32_t>(read32(bmpFS));
    h = static_cast<int32_t>(read32(bmpFS));
    read16(bmpFS); // color planes (must be 1)
    bitDepth = read16(bmpFS);

    if (!ensureRange(w, 1, TFT_WIDTH) || !ensureRange(h, 1, TFT_HEIGHT))
    {
      TFT_GUARD_LOG("BMP size %ldx%ld exceeds TFT bounds %ux%u\n", static_cast<long>(w), static_cast<long>(h), TFT_WIDTH, TFT_HEIGHT);
      loadOk = false;
    }
  }

  int32_t x = 0;
  int32_t y = 0;
  if (loadOk)
  {
    x = (TFT_WIDTH - w) / 2;
    y = (TFT_HEIGHT - h) / 2;

    if (!ensureRange(x, 0, TFT_WIDTH - w) || !ensureRange(y, 0, TFT_HEIGHT - h))
    {
      TFT_GUARD_LOG("BMP offset invalid: x=%ld y=%ld\n", static_cast<long>(x), static_cast<long>(y));
      loadOk = false;
    }
  }

#ifdef DEBUG_OUTPUT_IMAGES
  if (loadOk)
  {
    Serial.print(" image W, H, BPP: ");
    Serial.print(w);
    Serial.print(", ");
    Serial.print(h);
    Serial.print(", ");
    Serial.println(bitDepth);
    Serial.print(" dimming: ");
    Serial.println(dimming);
    Serial.print(" offset x, y: ");
    Serial.print(x);
    Serial.print(", ");
    Serial.println(y);
  }
#endif

  if (loadOk)
  {
    uint32_t compression = read32(bmpFS);
    if (compression != 0 || (bitDepth != 24 && bitDepth != 1 && bitDepth != 4 && bitDepth != 8))
    {
      Serial.println("BMP format not recognized.");
      TFT_GUARD_LOG("Unsupported BMP compression %lu or bit depth %u in %s\n", static_cast<unsigned long>(compression), bitDepth, filename);
      loadOk = false;
    }
  }

  uint32_t palette[kMaxBmpPaletteEntries];
  if (loadOk && bitDepth <= 8)
  {
    read32(bmpFS);
    read32(bmpFS);
    read32(bmpFS); // size, w resolution, h resolution
    paletteSize = read32(bmpFS);
    if (paletteSize == 0)
    {
      paletteSize = 1u << bitDepth;
    }
    if (paletteSize > kMaxBmpPaletteEntries)
    {
      TFT_GUARD_LOG("Palette size %lu exceeds buffer in %s\n", static_cast<unsigned long>(paletteSize), filename);
      loadOk = false;
    }
    else if (!bmpFS.seek(14 + headerSize))
    {
      TFT_GUARD_LOG("Seek to palette failed for %s\n", filename);
      loadOk = false;
    }
    else
    {
      for (uint32_t i = 0; i < paletteSize; i++)
      {
        palette[i] = read32(bmpFS);
      }
    }
  }

  if (loadOk && !bmpFS.seek(seekOffset))
  {
    TFT_GUARD_LOG("Seek to pixel data failed for %s\n", filename);
    loadOk = false;
  }

  uint32_t lineSize = 0;
  if (loadOk)
  {
    lineSize = ((bitDepth * w + 31) >> 5) * 4;
    if (lineSize == 0)
    {
      TFT_GUARD_LOG("Computed line size is zero for %s\n", filename);
      loadOk = false;
    }
  }

  if (loadOk)
  {
    uint8_t lineBuffer[lineSize];

    for (row = h - 1; row >= 0; row--)
    {
      int bytesRead = bmpFS.read(lineBuffer, lineSize);
      if (bytesRead != static_cast<int>(lineSize))
      {
        TFT_GUARD_LOG("Short read (%d/%lu) in row %ld for %s\n", bytesRead, static_cast<unsigned long>(lineSize), static_cast<long>(row), filename);
        loadOk = false;
        break;
      }

      uint8_t *bptr = lineBuffer;

      for (col = 0; col < w; col++)
      {
        uint16_t r = 0;
        uint16_t g = 0;
        uint16_t b = 0;

        if (bitDepth == 24)
        {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
        }
        else
        {
          uint32_t c = 0;
          if (bitDepth == 8)
          {
            c = palette[*bptr++];
          }
          else if (bitDepth == 4)
          {
            uint8_t idx = (*bptr >> ((col & 0x01) ? 0 : 4)) & 0x0F;
            c = palette[idx];
            if (col & 0x01)
            {
              bptr++;
            }
          }
          else
          { // bitDepth == 1
            uint8_t idx = (*bptr >> (7 - (col & 0x07))) & 0x01;
            c = palette[idx];
            if ((col & 0x07) == 0x07)
            {
              bptr++;
            }
          }
          b = static_cast<uint16_t>(c & 0xFF);
          g = static_cast<uint16_t>((c >> 8) & 0xFF);
          r = static_cast<uint16_t>((c >> 16) & 0xFF);
        }

        uint16_t color = static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xFF) >> 3));
#ifndef DIM_WITH_ENABLE_PIN_PWM
        if (dimming < 255)
        {
          color = alphaBlend(dimming, color, TFT_BLACK);
        }
#endif

        UnpackedImageBuffer[row + y][col + x] = color;
      }
    }
  }

  if (loadOk)
  {
    FileInBuffer = file_index;
  }
  else
  {
    FileInBuffer = 255;
  }

  bmpFS.close();

#if defined(DEBUG_OUTPUT_IMAGES) || defined(DEBUG_TFT_TIMING)
  const uint32_t loadElapsed = millis() - StartTime;
#endif

#ifdef DEBUG_OUTPUT_IMAGES
  if (loadOk)
  {
    Serial.print("img load time: ");
    Serial.println(loadElapsed);
  }
#endif

#ifdef DEBUG_TFT_TIMING
  if (loadOk)
  {
    timing.lastLoadMs = loadElapsed;
    if (loadElapsed > timing.maxLoadMs)
    {
      timing.maxLoadMs = loadElapsed;
    }
    timing.totalLoadMs += loadElapsed;
    ++timing.loadCount;
    Serial.printf("[TFT TIMING] load %s -> %lu ms (max %lu ms, n=%lu)\n",
                  filename,
                  static_cast<unsigned long>(loadElapsed),
                  static_cast<unsigned long>(timing.maxLoadMs),
                  static_cast<unsigned long>(timing.loadCount));
  }
#endif

  return loadOk;
}
#endif

#ifdef USE_CLK_FILES

int8_t TFTs::CountNumberOfClockFaces()
{
  int8_t i, found;
  char filename[10];

  Serial.print("Searching for CLK clock files... ");
  found = 0;
  for (i = 1; i < 10; i++)
  {
    sprintf(filename, "/%d.clk", i * 10); // search for files 10.clk, 20.clk,...
    if (!FileExists(filename))
    {
      found = i - 1;
      break;
    }
  }
  Serial.print(found);
  Serial.println(" fonts found.");
  return found;
}

bool TFTs::LoadImageIntoBuffer(uint8_t file_index)
{
  uint32_t StartTime = millis();

  fs::File bmpFS;
  // Filenames are no bigger than "255.clk\0"
  char filename[10];
  snprintf(filename, sizeof(filename), "/%u.clk", static_cast<unsigned int>(file_index));

#ifdef DEBUG_OUTPUT_IMAGES
  Serial.print("Loading: ");
  Serial.println(filename);
#endif

  // Open requested file on SD card
  bmpFS = SPIFFS.open(filename, "r");
  if (!bmpFS)
  {
    Serial.print("File not found: ");
    Serial.println(filename);
    TFT_GUARD_LOG("SPIFFS open failed for %s\n", filename);
    return false;
  }

  bool loadOk = true;
  int32_t w = 0;
  int32_t h = 0;
  int32_t row = 0;
  int32_t col = 0;

  // black background - clear whole buffer
  memset(UnpackedImageBuffer, '\0', sizeof(UnpackedImageBuffer));

  uint16_t magic = read16(bmpFS);
  if (magic == 0xFFFF)
  {
    Serial.print("Can't openfile. Make sure you upload the SPIFFs image with images. : ");
    Serial.println(filename);
    bmpFS.close();
    TFT_GUARD_LOG("CLK magic read failed for %s\n", filename);
    return false;
  }

  if (magic != 0x4B43)
  { // look for "CK" header
    Serial.print("File not a CLK. Magic: ");
    Serial.println(magic);
    bmpFS.close();
    TFT_GUARD_LOG("Unexpected CLK magic 0x%04X in %s\n", magic, filename);
    return false;
  }

  w = read16(bmpFS);
  h = read16(bmpFS);
  if (!ensureRange(w, 1, TFT_WIDTH) || !ensureRange(h, 1, TFT_HEIGHT))
  {
    TFT_GUARD_LOG("CLK size %ldx%ld exceeds TFT bounds %ux%u\n", static_cast<long>(w), static_cast<long>(h), TFT_WIDTH, TFT_HEIGHT);
    loadOk = false;
  }

  // center image on the display
  int32_t x = 0;
  int32_t y = 0;
  if (loadOk)
  {
    x = (TFT_WIDTH - w) / 2;
    y = (TFT_HEIGHT - h) / 2;
    if (!ensureRange(x, 0, TFT_WIDTH - w) || !ensureRange(y, 0, TFT_HEIGHT - h))
    {
      TFT_GUARD_LOG("CLK offset invalid: x=%ld y=%ld\n", static_cast<long>(x), static_cast<long>(y));
      loadOk = false;
    }
  }

#ifdef DEBUG_OUTPUT_IMAGES
  Serial.print(" image W, H: ");
  Serial.print(w);
  Serial.print(", ");
  Serial.println(h);
  Serial.print(" dimming: ");
  Serial.println(dimming);
  Serial.print(" offset x, y: ");
  Serial.print(x);
  Serial.print(", ");
  Serial.println(y);
#endif

  if (!loadOk)
  {
    bmpFS.close();
    FileInBuffer = 255;
    return false;
  }

  uint32_t lineSize = static_cast<uint32_t>(w) * 2u;
  if (lineSize == 0)
  {
    TFT_GUARD_LOG("CLK line size is zero for %s\n", filename);
    bmpFS.close();
    FileInBuffer = 255;
    return false;
  }

  uint8_t lineBuffer[lineSize];

  // 0,0 coordinates are top left
  for (row = 0; row < h; row++)
  {
    int bytesRead = bmpFS.read(lineBuffer, lineSize);
    if (bytesRead != static_cast<int>(lineSize))
    {
      TFT_GUARD_LOG("CLK short read (%d/%lu) in row %ld for %s\n", bytesRead, static_cast<unsigned long>(lineSize), static_cast<long>(row), filename);
      loadOk = false;
      break;
    }
    uint8_t PixM, PixL;

    // Colors are already in 16-bit R5, G6, B5 format
    for (col = 0; col < w; col++)
    {
#ifdef DIM_WITH_ENABLE_PIN_PWM
      // skip alpha blending for dimming if hardware dimming is used
      UnpackedImageBuffer[row + y][col + x] = (lineBuffer[col * 2 + 1] << 8) | (lineBuffer[col * 2]);
#else
      if (dimming == 255)
      { // not needed, copy directly
        UnpackedImageBuffer[row + y][col + x] = (lineBuffer[col * 2 + 1] << 8) | (lineBuffer[col * 2]);
      }
      else
      {
        uint16_t r = 0;
        uint16_t g = 0;
        uint16_t b = 0;
        // 16 BPP pixel format: R5, G6, B5 ; bin: RRRR RGGG GGGB BBBB
  PixM = lineBuffer[col * 2 + 1];
  PixL = lineBuffer[col * 2];
        // align to 8-bit value (MSB left aligned)
        r = (PixM) & 0xF8;
        g = ((PixM << 5) | (PixL >> 3)) & 0xFC;
        b = (PixL << 3) & 0xF8;
        r *= dimming;
        g *= dimming;
        b *= dimming;
        r = r >> 8;
        g = g >> 8;
        b = b >> 8;
        UnpackedImageBuffer[row + y][col + x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      } // dimming
#endif
    } // col
  } // row
  if (loadOk)
  {
    FileInBuffer = file_index;
  }
  else
  {
    FileInBuffer = 255;
  }

  bmpFS.close();
#if defined(DEBUG_OUTPUT_IMAGES) || defined(DEBUG_TFT_TIMING)
  const uint32_t loadElapsed = millis() - StartTime;
#endif
#ifdef DEBUG_OUTPUT_IMAGES
  if (loadOk)
  {
    Serial.print("img load time: ");
    Serial.println(loadElapsed);
  }
#endif
#ifdef DEBUG_TFT_TIMING
  if (loadOk)
  {
    timing.lastLoadMs = loadElapsed;
    if (loadElapsed > timing.maxLoadMs)
    {
      timing.maxLoadMs = loadElapsed;
    }
    timing.totalLoadMs += loadElapsed;
    ++timing.loadCount;
    Serial.printf("[TFT TIMING] load %s -> %lu ms (max %lu ms, n=%lu)\n",
                  filename,
                  static_cast<unsigned long>(loadElapsed),
                  static_cast<unsigned long>(timing.maxLoadMs),
                  static_cast<unsigned long>(timing.loadCount));
  }
#endif
  return loadOk;
}
#endif

void TFTs::DrawImage(uint8_t file_index)
{

  uint32_t StartTime = millis();
#ifdef DEBUG_OUTPUT_IMAGES
  Serial.println("");
  Serial.print("Drawing image: ");
  Serial.println(file_index);
#endif
  // check if file is already loaded into buffer; skip loading if it is. Saves 50 to 150 msec of time.
  if (file_index != FileInBuffer)
  {
#ifdef DEBUG_OUTPUT_IMAGES
    Serial.println("Not preloaded; loading now...");
#endif
    if (!LoadImageIntoBuffer(file_index))
    {
      TFT_GUARD_LOG("DrawImage abort: loading %u failed\n", static_cast<unsigned int>(file_index));
      return;
    }
  }

  bool oldSwapBytes = getSwapBytes();
  setSwapBytes(true);
  pushImage(0, 0, TFT_WIDTH, TFT_HEIGHT, reinterpret_cast<uint16_t *>(UnpackedImageBuffer));
  setSwapBytes(oldSwapBytes);

#ifdef DEBUG_OUTPUT_IMAGES
  Serial.print("img transfer time: ");
  Serial.println(millis() - StartTime);
#endif
#ifdef DEBUG_TFT_TIMING
  uint32_t drawElapsed = millis() - StartTime;
  timing.lastDrawMs = drawElapsed;
  if (drawElapsed > timing.maxDrawMs)
  {
    timing.maxDrawMs = drawElapsed;
  }
  timing.totalDrawMs += drawElapsed;
  ++timing.drawCount;
  Serial.printf("[TFT TIMING] draw %u -> %lu ms (max %lu ms, n=%lu)\n",
                static_cast<unsigned int>(file_index),
                static_cast<unsigned long>(drawElapsed),
                static_cast<unsigned long>(timing.maxDrawMs),
                static_cast<unsigned long>(timing.drawCount));
#endif
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t TFTs::read16(fs::File &f)
{
  uint16_t result;
  reinterpret_cast<uint8_t *>(&result)[0] = f.read(); // LSB
  reinterpret_cast<uint8_t *>(&result)[1] = f.read(); // MSB
  return result;
}

uint32_t TFTs::read32(fs::File &f)
{
  uint32_t result;
  reinterpret_cast<uint8_t *>(&result)[0] = f.read(); // LSB
  reinterpret_cast<uint8_t *>(&result)[1] = f.read();
  reinterpret_cast<uint8_t *>(&result)[2] = f.read();
  reinterpret_cast<uint8_t *>(&result)[3] = f.read(); // MSB
  return result;
}

String TFTs::clockFaceToName(uint8_t clockFace)
{
  return patterns_str[clockFace - 1];
}

uint8_t TFTs::nameToClockFace(String name)
{
  for (int i = 0; i < 9; i++)
  {
    if (patterns_str[i] == name)
    {
      return i + 1;
    }
  }
  return 1;
}
#ifdef DEBUG_TFT_TIMING
void TFTs::ResetTimingStats()
{
  timing = TimingStats{};
}

void TFTs::PrintTimingStats() const
{
  uint32_t avgLoad = timing.loadCount ? static_cast<uint32_t>(timing.totalLoadMs / timing.loadCount) : 0;
  uint32_t avgDraw = timing.drawCount ? static_cast<uint32_t>(timing.totalDrawMs / timing.drawCount) : 0;

  Serial.printf("[TFT TIMING] loads: last=%lu ms, avg=%lu ms, max=%lu ms, n=%lu\n",
                static_cast<unsigned long>(timing.lastLoadMs),
                static_cast<unsigned long>(avgLoad),
                static_cast<unsigned long>(timing.maxLoadMs),
                static_cast<unsigned long>(timing.loadCount));
  Serial.printf("[TFT TIMING] draws: last=%lu ms, avg=%lu ms, max=%lu ms, n=%lu\n",
                static_cast<unsigned long>(timing.lastDrawMs),
                static_cast<unsigned long>(avgDraw),
                static_cast<unsigned long>(timing.maxDrawMs),
                static_cast<unsigned long>(timing.drawCount));
}
#endif
//// END STOLEN CODE
