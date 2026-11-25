# Wokwi simulation (EleksTube IPS clock)

Simulates the original **EleksTube** clock in the [Wokwi](https://wokwi.com) VS Code extension:
ESP32, six 135x240 ST7789 displays (one custom chip with six panels), 74HC595 chip select,
RTC, six WS2812 backlights and the four buttons. No real hardware needed.

## Files

| File | Purpose |
|------|---------|
| `../wokwi.toml` | Tells the extension which firmware/ELF and custom chip to load |
| `../diagram.json` | The simulated circuit (wiring as in `GLOBAL_DEFINES.h`, `HARDWARE_ELEKSTUBE_CLOCK`) |
| `platformio_wokwi.ini` | Extra PlatformIO config with env `EleksTube_Wokwi` (not built by CI) |
| `script_wokwi_*.py` | Fixed output names + merged flash image incl. LittleFS clock faces |
| `../include/_USER_DEFINES_WOKWI.h` | Template defines + simulator WiFi `Wokwi-GUEST` (no personal values) |
| `chips/st7789x6.chip.*` | Six ST7789 panels in one chip (C source, pin/display definition, compiled wasm) |
| `build_chip.py` | Compiles `chips/*.chip.c` to WebAssembly |

## Usage

1. Build the firmware (from the project root):

   ```bash
   pio run -c wokwi/platformio_wokwi.ini -e EleksTube_Wokwi
   ```

   Output: `.pio/build_wokwi/EleksTube_Wokwi/wokwi_firmware.elf` and `wokwi_flash.bin`
   (bootloader + partitions + app + LittleFS in one 4MB image).
2. In VS Code: `F1` -> **Wokwi: Start Simulator**.
3. Buttons: click them or use the keys `l` (left), `m` (mode), `r` (right), `p` (power)
   while the simulator has the focus.

The simulator connects to `Wokwi-GUEST` and reaches the internet through a Wokwi gateway,
so NTP and geolocation work wherever the PC is online. With the public gateway the IP
geolocation reports the gateway's location (e.g. a US time zone), not yours.

Headless run (serial output + screenshot of the displays), needs `WOKWI_CLI_TOKEN`:

```bash
wokwi-cli --timeout 25000 --screenshot-part tubes --screenshot-time 24000 .
```

## Differences to the real clock

- **RTC**: Wokwi has no DS3231, a DS1307 is used (same I2C address and time registers).
  `getTemperature()` returns nonsense.
- **Web setup portal**: the env removes `WEB_SETUP_PORTAL` (the simulator has no soft AP),
  so it is the classic build with fixed WiFi credentials.
- **Displays**: write-only emulation (no `readPixel`), 16 bit colour only, no scrolling.
  All six panels live in one custom chip: the firmware selects several panels at once
  (broadcast init) and Wokwi does not handle several custom chips receiving on the same
  SPI bus at the same time.
- Backlight LED order of the real board is assumed as pixel 0 = seconds ones (rightmost).

## Rebuilding the display chip

Only needed after changing `chips/st7789x6.chip.c`. One-time toolchain setup (Zig C compiler
from PyPI, kept in the git-ignored `.pio` folder):

```bash
python -m venv .pio/wokwi-toolchain
.pio/wokwi-toolchain/Scripts/python -m pip install ziglang   # Linux/macOS: .pio/wokwi-toolchain/bin/python
.pio/wokwi-toolchain/Scripts/python wokwi/build_chip.py
```
