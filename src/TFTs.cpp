#include "MQTT_client_ips.h"
#include "TFTs.h"
#include "WiFi_WPS.h"

void TFTs::begin()
{
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin()");
#endif
  chip_select.begin();
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - chip_select.begin() finished.");
  delay(2000);
  Serial.println("TFTs::begin() - Calling chip_select.setAll()");
#endif
  chip_select.setAll(); // Start with all displays selected
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - chip_select.setAll() finished.");
  delay(2000);
#endif

#ifdef DIM_WITH_ENABLE_PIN_PWM
  // If hardware dimming is used, init ledc, set the pin and channel for PWM and set frequency and resolution
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - Using hardware dimming with PWM.");
  Serial.print("TFTs::begin() - TFT_ENABLE_PIN: ");
  Serial.println(TFT_ENABLE_PIN);
  Serial.print("TFTs::begin() - TFT_PWM_CHANNEL: ");
  Serial.println(TFT_PWM_CHANNEL);
#endif
  ledcSetup(TFT_PWM_CHANNEL, TFT_PWM_FREQ, TFT_PWM_RESOLUTION);           // PWM, globally defined
  ledcAttachPin(TFT_ENABLE_PIN, TFT_PWM_CHANNEL);                         // Attach the pin to the PWM channel
  ledcChangeFrequency(TFT_PWM_CHANNEL, TFT_PWM_FREQ, TFT_PWM_RESOLUTION); // need to set the frequency and resolution again to have the hardware dimming working properly
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - PWM setup finished.");
#endif
#else
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - Not using hardware dimming with PWM.");
  Serial.print("TFTs::begin() - TFT_ENABLE_PIN: ");
  Serial.println(TFT_ENABLE_PIN);  
#endif
  pinMode(TFT_ENABLE_PIN, OUTPUT); // Set pin for turning display power on and off.
#endif
  Serial.println("TFTs::begin() - After setting up TFT_ENABLE_PIN.");
  delay(2000);
  Serial.println("TFTs::begin() - Calling InvalidateImageInBuffer()");
  InvalidateImageInBuffer(); // Signal, that the image in the buffer is invalid and needs to be reloaded and refilled
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - InvalidateImageInBuffer() finished.");

  Serial.println("TFTs::begin() - Calling init()");
#endif
  init();                    // Initialize the super class.
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - init() finished.");
  delay(2000);
  Serial.println("TFTs::begin() - Calling reclaimPins()");
#endif
#if defined(HARDWARE_MARVELTUBES_CLOCK)
  chip_select.reclaimPins(); // regain control of per-digit CS pins after TFT_eSPI::init()
  Serial.println("TFTs::begin() - After reclaimPins()");
  delay(2000);
  Serial.println("TFTs::begin() - Calling chip_select.setAll()");
  chip_select.setAll(); // After regain control, start with all displays selected again
  Serial.println("TFTs::begin() - chip_select.setAll() finished.");
  delay(2000);
#endif

#ifdef TFT_INIT_RED_ONLY
  Serial.println("TFTs::begin() - Calling fillScreen(TFT_RED)");
  fillScreen(TFT_RED);     // minimal-test mode: show solid red immediately
  Serial.println("TFTs::begin() - fillScreen(TFT_RED) finished.");
  delay(2000);
#else
  Serial.println("TFTs::begin() - Calling fillScreen(TFT_BLACK)");
  fillScreen(TFT_BLACK);     // to avoid/reduce flickering patterns on the screens
  Serial.println("TFTs::begin() - fillScreen(TFT_BLACK) finished.");
#endif

#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - Calling enableAllDisplays()");
#endif
  enableAllDisplays();       // Signal, that the displays are enabled now and do the hardware dimming, if available and enabled
  delay(2000);
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - enableAllDisplays() finished.");
#endif

#ifdef TFT_INIT_RED_ONLY
  NumberOfClockFaces = 0; // skip any further assets in minimal test mode
  
  
  
  
  
  return;




  #endif

  if (!SPIFFS.begin()) // Initialize SPIFFS
  {
    Serial.println("SPIFFS initialization failed!");
#ifdef DEBUG_OUTPUT_TFT
    Serial.println("TFTs::begin() - Finished with SPIFFS initialization failed!");
#endif
    NumberOfClockFaces = 0;
    return;
  }

#ifdef DEBUG_OUTPUT_TFT
  Serial.println("SPIFFS initialized successfully.");
  Serial.println("TFTs::begin() - Calling CountNumberOfClockFaces()");
#endif
  NumberOfClockFaces = CountNumberOfClockFaces();
#ifdef DEBUG_OUTPUT_TFT
  Serial.print("TFTs::begin() - NumberOfClockFaces: ");
  Serial.println(NumberOfClockFaces);
  Serial.println("TFTs::begin() - Calling loadClockFacesNames()");
#endif
  loadClockFacesNames();
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::begin() - loadClockFacesNames() finished.");
  Serial.println("TFTs::begin() - Finished!");
#endif
}

void TFTs::reinit()
{
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::reinit()");
#endif
  if (!TFTsEnabled) // perform re-init only if displays are actually off. HA sends ON command together with clock face change which causes flickering.
  {
#ifndef TFT_SKIP_REINIT
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::reinit() - Reinitializing displays.");
#endif
    // Start with all displays selected.
    chip_select.begin();
    chip_select.setAll(); // Start again with all displays selected.

#ifdef DIM_WITH_ENABLE_PIN_PWM
    ledcAttachPin(TFT_ENABLE_PIN, TFT_PWM_CHANNEL);
    ledcChangeFrequency(TFT_PWM_CHANNEL, TFT_PWM_FREQ, TFT_PWM_RESOLUTION);
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::reinit() - PWM reconfiguration finished.");
#endif
#else
    pinMode(TFT_ENABLE_PIN, OUTPUT); // Set pin for turning display power on and off.
#endif
    InvalidateImageInBuffer(); // Signal, that the image in the buffer is invalid and needs to be reloaded and refilled
    init();                    // Initialize the super class (again).
    chip_select.reclaimPins();
    fillScreen(TFT_BLACK);     // to avoid/reduce flickering patterns on the screens
    enableAllDisplays();       // Signal, that the displays are enabled now
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::reinit() - Full reinitialization finished.");
#endif
#else                          // TFT_SKIP_REINIT
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::reinit() - Skipping full initialization, just re-enabling displays.");
#endif
    enableAllDisplays(); // skip full inintialization, just reenable displays by signaling to enable them
#endif                         // TFT_SKIP_REINIT
  }
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::reinit() - Finished!");
#endif
}

void TFTs::clear()
{
  // Start with all displays selected.
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::clear()");
  Serial.println("TFTs::clear() - Calling chip_select.setAll()");
#endif
  chip_select.setAll();
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::clear() - chip_select.setAll() finished.");
  Serial.println("TFTs::clear() - Calling enableAllDisplays()");
#endif
  enableAllDisplays();
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::clear() - enableAllDisplays() finished.");
  Serial.println("TFTs::clear() - Finished!");
#endif
}

void TFTs::loadClockFacesNames()
{
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::loadClockFacesNames()");
#endif
  int8_t i = 0;
  const char *filename = "/clockfaces.txt";
  Serial.println("Loading clock face names...");
  fs::File f = SPIFFS.open(filename);
  if (!f)
  {
    Serial.println("ERROR: SPIFFS clockfaces.txt not found.");
#ifdef DEBUG_OUTPUT_TFT
    Serial.println("TFTs::loadClockFacesNames() - Failed to open file.");
#endif
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
#ifdef DEBUG_OUTPUT_TFT
  Serial.println("TFTs::loadClockFacesNames() - Finished!");
#endif
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
  Serial.println("TFTs::enableAllDisplays() - Enabling displays via TFT_ENABLE_PIN.");
  digitalWrite(TFT_ENABLE_PIN, ACTIVATEDISPLAYS);
  Serial.println("TFTs::enableAllDisplays() - Displays enabled.");
  delay(2000);  
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
  Serial.println("TFTs::disableAllDisplays() - Disabling displays via TFT_ENABLE_PIN.");
  digitalWrite(TFT_ENABLE_PIN, DEACTIVATEDISPLAYS);
  Serial.println("TFTs::disableAllDisplays() - Displays disabled.");
  delay(2000);
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
      Serial.print("TFTs::setDigit() - call showDigit(");
      Serial.print(digit);
      Serial.println(")");
      showDigit(digit);
      Serial.println("TFTs::setDigit() - showDigit() finished.");

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
#ifdef DEBUG_OUTPUT_TFT
  Serial.print("TFTs::showDigit(");
  Serial.print(digit);
  Serial.println(")");
#endif
  if (TFTsEnabled)
  { // only do this, if the displays are enabled
#ifdef DEBUG_OUTPUT_TFT
    Serial.print("TFTs::showDigit() - Displays enabled, showing digit: ");
    Serial.println(digit);
    Serial.println("TFTs::showDigit() - Calling chip_select.setDigit()");
#endif
    chip_select.setDigit(digit);
#ifdef DEBUG_OUTPUT_TFT
    Serial.println("TFTs::showDigit() - chip_select.setDigit() finished.");
#endif

    if (digits[digit] == blanked)
    { // Blank Zero
#ifdef DEBUG_OUTPUT_TFT
      Serial.println("TFTs::showDigit() - Digit blanked, clearing screen.");
#endif
      fillScreen(TFT_BLACK);
    }
    else
    {
#ifdef DEBUG_OUTPUT_TFT
      Serial.print("TFTs::showDigit() - Digit value: ");
      Serial.println(digits[digit]);
#endif
      uint8_t file_index = current_graphic * 10 + digits[digit];
#ifdef DEBUG_OUTPUT_IMAGES
      Serial.print("TFTs::showDigit() - file_index: ");
      Serial.println(file_index);
      Serial.println("TFTs::showDigit() - Calling DrawImage()");
#endif
      DrawImage(file_index);
#ifdef DEBUG_OUTPUT_IMAGES
      Serial.println("TFTs::showDigit() - DrawImage() finished.");
#endif

      uint8_t NextNumber = digits[SECONDS_ONES] + 1;
      if (NextNumber > 9)
        NextNumber = 0; // pre-load only seconds, because they are drawn first
      NextFileRequired = current_graphic * 10 + NextNumber;
    }
#if defined(HARDWARE_IPSTUBE_CLOCK) || defined(HARDWARE_MARVELTUBES_CLOCK)
#ifdef DEBUG_OUTPUT_TFT
    Serial.println("TFTs::showDigit() - Calling chip_select.update()");
#endif
    chip_select.update();
#endif
  }
#ifdef DEBUG_OUTPUT_TFT
  else
  {
    Serial.println("TFTs::showDigit() - Displays disabled, skipping update.");
  }
  Serial.println("TFTs::showDigit() - Finished!");
#endif
  // else { } //display is disabled, do nothing
}

void TFTs::LoadNextImage()
{
  if (NextFileRequired != FileInBuffer)
  {
#ifdef DEBUG_OUTPUT_IMAGES
    Serial.println("Preload next img");
#endif
    LoadImageIntoBuffer(NextFileRequired);
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
#ifdef DEBUG_OUTPUT_IMAGES
  Serial.print("FileExists check for ");
  Serial.print(path);
  Serial.print(": ");
  Serial.println(Exists ? "true" : "false");
#endif
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
  sprintf(filename, "/%d.bmp", file_index);

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
    return (false);
  }

  uint32_t seekOffset, headerSize, paletteSize = 0;
  int16_t w, h, row, col;
  uint16_t r, g, b, bitDepth;

  // black background - clear whole buffer
  memset(UnpackedImageBuffer, '\0', sizeof(UnpackedImageBuffer));

  uint16_t magic = read16(bmpFS);
  if (magic == 0xFFFF)
  {
    Serial.print("Can't openfile. Make sure you upload the SPIFFs image with BMPs. : ");
    Serial.println(filename);
    bmpFS.close();
    return (false);
  }

  if (magic != 0x4D42)
  {
    Serial.print("File not a BMP. Magic: ");
    Serial.println(magic);
    bmpFS.close();
    return (false);
  }

  read32(bmpFS);              // filesize in bytes
  read32(bmpFS);              // reserved
  seekOffset = read32(bmpFS); // start of bitmap
  headerSize = read32(bmpFS); // header size
  w = read32(bmpFS);          // width
  h = read32(bmpFS);          // height
  read16(bmpFS);              // color planes (must be 1)
  bitDepth = read16(bmpFS);

  // center image on the display
  int16_t x = (TFT_WIDTH - w) / 2;
  int16_t y = (TFT_HEIGHT - h) / 2;

#ifdef DEBUG_OUTPUT_IMAGES
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
#endif
  if (read32(bmpFS) != 0 || (bitDepth != 24 && bitDepth != 1 && bitDepth != 4 && bitDepth != 8))
  {
    Serial.println("BMP format not recognized.");
    bmpFS.close();
    return (false);
  }

  uint32_t palette[256];
  if (bitDepth <= 8) // 1,4,8 bit bitmap: read color palette
  {
    read32(bmpFS);
    read32(bmpFS);
    read32(bmpFS); // size, w resolution, h resolution
    paletteSize = read32(bmpFS);
    if (paletteSize == 0)
      paletteSize = pow(2, bitDepth); // if 0, size is 2^bitDepth
    bmpFS.seek(14 + headerSize);      // start of color palette
    for (uint16_t i = 0; i < paletteSize; i++)
    {
      palette[i] = read32(bmpFS);
    }
  }

  bmpFS.seek(seekOffset);

  uint32_t lineSize = ((bitDepth * w + 31) >> 5) * 4;
  uint8_t lineBuffer[lineSize];

  // row is decremented as the BMP image is drawn bottom up
  for (row = h - 1; row >= 0; row--)
  {
    bmpFS.read(lineBuffer, sizeof(lineBuffer));
    uint8_t *bptr = lineBuffer;

    // Convert 24 to 16 bit colours while copying to output buffer.
    for (col = 0; col < w; col++)
    {
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
          c = palette[(*bptr >> ((col & 0x01) ? 0 : 4)) & 0x0F];
          if (col & 0x01)
            bptr++;
        }
        else
        { // bitDepth == 1
          c = palette[(*bptr >> (7 - (col & 0x07))) & 0x01];
          if ((col & 0x07) == 0x07)
            bptr++;
        }
        b = c;
        g = c >> 8;
        r = c >> 16;
      }

      uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xFF) >> 3);
#ifndef DIM_WITH_ENABLE_PIN_PWM // skip alpha blending for dimming if hardware dimming is used
      if (dimming < 255)
      { // only dim when needed
        color = alphaBlend(dimming, color, TFT_BLACK);
      } // dimming
#endif

      UnpackedImageBuffer[row + y][col + x] = color;
    } // col
  } // row
  FileInBuffer = file_index;

  bmpFS.close();
#ifdef DEBUG_OUTPUT_IMAGES
  Serial.print("img load time: ");
  Serial.println(millis() - StartTime);
#endif
  return (true);
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
  sprintf(filename, "/%d.clk", file_index);

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
    return (false);
  }

  int16_t w, h, row, col;
  uint16_t r, g, b;

  // black background - clear whole buffer
  memset(UnpackedImageBuffer, '\0', sizeof(UnpackedImageBuffer));

  uint16_t magic = read16(bmpFS);
  if (magic == 0xFFFF)
  {
    Serial.print("Can't openfile. Make sure you upload the SPIFFs image with images. : ");
    Serial.println(filename);
    bmpFS.close();
    return (false);
  }

  if (magic != 0x4B43)
  { // look for "CK" header
    Serial.print("File not a CLK. Magic: ");
    Serial.println(magic);
    bmpFS.close();
    return (false);
  }

  w = read16(bmpFS);
  h = read16(bmpFS);

  // center image on the display
  int16_t x = (TFT_WIDTH - w) / 2;
  int16_t y = (TFT_HEIGHT - h) / 2;

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

  uint8_t lineBuffer[w * 2];

  // 0,0 coordinates are top left
  for (row = 0; row < h; row++)
  {
    bmpFS.read(lineBuffer, sizeof(lineBuffer));
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
  FileInBuffer = file_index;

  bmpFS.close();
#ifdef DEBUG_OUTPUT_IMAGES
  Serial.print("img load time: ");
  Serial.println(millis() - StartTime);
#endif
  return (true);
}
#endif

void TFTs::DrawImage(uint8_t file_index)
{

  uint32_t StartTime = millis();
#ifdef DEBUG_OUTPUT_IMAGES
  Serial.print("TFTs::DrawImage(");
  Serial.print(file_index);
  Serial.println(")");
#endif
  // check if file is already loaded into buffer; skip loading if it is. Saves 50 to 150 msec of time.
  if (file_index != FileInBuffer)
  {
#ifdef DEBUG_OUTPUT_IMAGES
  Serial.println("TFTs::DrawImage() - Not preloaded, loading now...");
#endif
    LoadImageIntoBuffer(file_index);
  }

  bool oldSwapBytes = getSwapBytes();
  setSwapBytes(true);
  pushImage(0, 0, TFT_WIDTH, TFT_HEIGHT, reinterpret_cast<uint16_t *>(UnpackedImageBuffer));
  setSwapBytes(oldSwapBytes);

#ifdef DEBUG_OUTPUT_IMAGES
  Serial.print("img transfer time: ");
  Serial.println(millis() - StartTime);
  Serial.println("TFTs::DrawImage() - Finished!");
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
//// END STOLEN CODE
