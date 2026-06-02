# MarvelTubes Gen2 Clock

## Identified components

- MCU **ESP32** as ESP32-WROOM-32E module
  - Standard ESP32 (Xtensa LX6 dual-core, ROM: "ets Jul 29 2019 12:21:46")
  - Flash **16MB** built into the module (no external flash chip)
  - No PSRAM
- UART Chip **CH340** as CH340C, see [datasheet](https://web.archive.org/web/20230328023924/http://wch-ic.com/downloads/file/79.html?time=2023-01-31%2005:37:01&code=byrlQteadwoMguMjmbWlPKyCiEACwGGapSxnN8I1)
- Audio **NAU88C22** 24-bit Stereo Audio Codec with Speaker Driver [datasheet](https://www.nuvoton.com/resource-files/NAU88C22DataSheet0.6.pdf)
  - Control interface: **I2C or 3-wire SPI**, selected by the CSB pin on the PCB
    - CSB pulled HIGH → I2C mode, 7-bit address 0x1A (CSAD=0) or 0x1B (CSAD=1)
    - CSB pulled LOW → 3-wire SPI mode
  - Audio output: I2S (BCLK, LRCK/WS, SDIN) — exact GPIO assignments unknown, need discovery
  - I2C/SPI GPIO assignments: unknown, need discovery
- **No RTC** — no real-time clock chip, no backup battery on Gen2
- Generic active buzzer (GPIO unknown, need discovery)
- **LCD panels** × 6
  - Generic FPC ST7789 panel (135×240), see [datasheet](https://www.buydisplay.com/download/manual/ER-TFT1.14-2_Datasheet.pdf)
  - Backlight LEDs: **WS2812B** × 6, see [datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf)
- Transistor for LCD backlight control (on/off and PWM dimming) — marked **A79T** (AO3407 P-channel MOSFET), see [datasheet](https://alltransistors.com/mosfet/transistor.php?transistor=16072)
  - Active LOW (GPIO26 drives gate; LOW = backlights ON)
- Default "auto download mode" circuit for ESP32 (EN + GPIO0 buttons)
- **AMS1085** voltage regulator, see [datasheet](http://www.advanced-monolithic.com/pdf/ds1085.pdf)

## GPIO Pin Assignment (confirmed)

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 4    | WS2812B data (backlights) | out | |
| 5    | LCD CS — Minutes Tens | out | CS_DIRECT_GPIO |
| 14   | LCD CS — Seconds Ones | out | CS_DIRECT_GPIO |
| 16   | TFT SPI SCLK | out | VSPI CLK |
| 17   | LCD CS — Minutes Ones | out | CS_DIRECT_GPIO |
| 18   | TFT SPI MOSI | out | VSPI MOSI |
| 19   | LCD CS — Hours Ones | out | CS_DIRECT_GPIO; was hijacked as VSPI MISO by TFT_eSPI — fixed by reclaimPins() |
| 21   | TFT DC (Data/Command) | out | |
| 22   | LCD CS — Hours Tens | out | CS_DIRECT_GPIO |
| 23   | TFT RST (Reset) | out | |
| 26   | TFT backlight enable (PWM dim) | out | Active LOW via AO3407; PWM channel 0 |
| 27   | LCD CS — Seconds Tens | out | CS_DIRECT_GPIO |
| 34   | Button LEFT (Style) | in-only | active LOW, external pull-up |
| 35   | Button MODE (Menu) | in-only | active LOW, external pull-up |
| 36   | Button POWER (Alarm) | in-only | active LOW, external pull-up |
| 39   | Button RIGHT (Time) | in-only | active LOW, external pull-up |

## Free output-capable GPIOs (unassigned)

`2`, `12`, `13`, `15`, `25`, `32`, `33`

These are candidates for: NAU88C22 I2C/SPI control lines, I2S audio lines (BCLK, LRCK, SDIN), buzzer.

## Firmware notes

- Flash partition: `partition_16MB.csv` — nvs@0x9000, app@0x10000 (2MB), littlefs@0x210000 (~13.3MB)
- Custom board definition required: `boards/esp32devmarveltubesgen2.json` (flash_size=16MB, flash_mode=dio)
- No `Wire.begin()` should be called at startup (no RTC) — otherwise I2C bus contention on TFT_DC/CS pins
- CS method: `CS_DIRECT_GPIO` — each display has its own GPIO, `reclaimPins()` must be called after `TFT_eSPI::init()`
