# EleksTubeHAX - An aftermarket custom firmware for the desk clock

![EleksTube IPS clock](/documentation/ImagesMD/EleksTube_IPS_Classic_Edition.jpg)

## Supported hardware models

**"EleksTube IPS clock" Original Version**
**"EleksTube IPS clock" Gen2 - now officially called "EleksTube IPS Classic Edition"**
**"SI HAI IPS clock"**
**"NovelLife SE clock" - with and without gesture sensor**
**"PunkCyber clock/RGB Glow Tube DIY clock" from pcbway**
**"IPSTUBE clock - Model H401" - with and without bottom LED stripe**

Note: All "Original EleksTube" clocks, sold after July 2022 are "Gen2" versions. See [Note on EleksTube website](https://elekstube.com/blogs/news/instructions-on-elekstube-clock-for-gen2-systems)

Buy your own clock under the name "EleksTube IPS clock" on eBay, Banggood, AliExpress, on the EleksTube website, etc.
Look for places that offer 30-day guarantee, otherwise it can be a fake product!

This firmware supports and was tested on different clocks!

Note that "EleksTube IPS" is the original, from the inventor of this type of the clock in 2021.
Later more and more similarly looking clock designs appeared, copies of the original with more or less modified hardware. Also newer versions from EleksTube itself are on the market (i.e. PR1 and PR2).

With this project we try to support the most popular models.

If you find another clone/version of this kind of clocks, please create an issue in the "Issues" section of this GitHub project and tells us details.

## Mainboard/PCB views

EleksTube IPS - Orginal Version - with hardware modification
![EleksTube IPS clock](/documentation/ImagesMD/EleksTube_original_PCB.jpg)
SI HAI IPS
![SI HAI IPS clock](/documentation/ImagesMD/SI_HAI_ips_clock.jpg)
NovelLife SE
![NovelLife SE clock](/documentation/ImagesMD/NovelLife_SE.jpg)
PunkCyber
![PunkCyber / RGB Glow tube](/documentation/ImagesMD/PunkCyber_ips_clock.jpg)
IPSTUBE - H401
![IPSTUBE clock - Model H401](/documentation/ImagesMD/IPSTUBE_H401_PCB2.jpg)

## Main clock features

- Multiple clock faces can be loaded into the clocks flash memory. Switchable via clock menu (or via MQTT messages)
- WiFi connectivity with NTP server synchronization
- Supports either WPS connection or hardcoded WiFi credentials  
- Manual time zone adjust in 15 minute/1 h slots
- RGB backlights (wall lights) for nice ambient light with multiple modes ("Off", "Test", "Constant", "Rainbow", "Pulse", "Breath")
- Dimming of the clock and backlights during the night time (configurable in code)
- Switching the displays on and off (not supported on all clocks)
- Different bitmap image files supported (BMP classic or paletized) and proprietary compressed files (CLK)
- Max image size is 135x240 (WxH) pixels
- Supports smaller images - will be automatically centered
- Advanced error handling for best user experience
- WiFi and MQTT errors are displayed below clock faces
- Optional MQTT client for remote controlling - clock faces switch and displays on/off can be controlled via MQTT messages. So with a broker, also via mobile phone (SmartNest, SmartThings, Google assistant, Alexa, etc.) or can be included into existing home automation network.
- Optional IP-based geolocation for automatic timezone and DST adjustments (only supported geolocation provider is "[Abstract](https://www.abstractapi.com/)")
- Optional DS18B20 temperature sensor to display temperature on the clock

## Work in progress

- Integrated web server to remote control the clock (and/or maybe load new clock faces)

## How to use this firmware

### Pre-Build images

If you just want to use this firmware without setting up all the tools and libraries and everything, navigate to folder `pre-built-firmware` and modify `_ESP32 write flash.cmd` to upload the firmware version for your clock.

See also the `README.MD` in the `pre-built-firmware` subfolder of the repo.

If you want more features and configure the firmware, check also section "How to build this firmware".

### Backup first

If you mess-up your clock, it's only your fault!

Backup images from other users **DO NOT WORK** as the original EleksTube firmware is locked by MAC address of ESP32.

For other clocks it MAY work, but don't assume it!

**Always backup YOUR clocks firmware version as first step!**

### Install the USB Serial Port Device Driver

Windows

- On Windows, plug-in the cable into the clock and connect it to an USB port of your PC. Then run Windows Update. It will find and install the driver and generate an virtual COM port.
- Only if Windows Update is not working for you, see the chinese manufacturers website for newest [CH340 driver](https://www.wch-ic.com/downloads/CH341SER_ZIP.html) and install them manually.

Linux

- On an up-to-date Linux it works out of the box.

### Save your original firmware

Windows users:

- In the folder "pre-built-firmware" of this repo you have the `esptool.exe`, which is used to talk to the ESP32 in your clock over the USB-Serial chip on the board of the clock.
- Open Device Manager and find out which virtual COM port represents your clock.
- Modify file `_ESP32 save flash 4MB.cmd` with your COM port number (or 8MB version for the IPSTUBE).
- Run the CMD file.
- It will generate a file named `backup1.bin`. Save it to a safe place!
- This is your precious backup.

Linux users:

- You probably already know where to get `Esptool` and how to use it. :)
- If not, this guide from the Tasmota project is very easy to follow: [ESPTool](https://tasmota.github.io/docs/Esptool/#put-device-in-firmware-upload-mode)
- Adopt the settings for your connected clock and save the firmware file to your device.

## How to build this firmware

Unfortunately, it's not simple plug-and-play.

You need to do some things.

These instructions assume you already know how to use the **Visual Studio Code** and the **PlatformIO IDE** extension for it, and just need to know what to do.

### Download this code

You're either reading this file after downloading/cloning it already, or you're reading it on GitHub.

If the last point applies, then we assume you can figure out how to get the code from GitHub and put it somewhere on your local machine.

### Visual Studio Code & PlatformIO IDE

Follow this guide here: [Install PlatformIO IDE](https://platformio.org/install/ide?install=vscode)

In short:

- Download, install and run VSCode
- Go to Extensions, search for "PlatformIO IDE" and install it (it will take a while - observe status messages in the bottom right corner).
If you don't have Python already installed it will be automatically added by PlatformIO. In case of issues, install Python 3.x manually.

#### ESP32 platform support

The EspressIF 32 development platform for PlatformIO is required to support the ESP32 microcontroller. It will be installed automatically when this project is opened in VSCode/PlatformIO or if the first build is triggered. It will take a while - observe status messages in the bottom right corner.

Tested on version 6.0.7 from the [PlatformIO registry](https://registry.platformio.org/platforms/platformio/espressif32).
The default environment for this project are using the "board" definition of the orginal "ESP32 development board" named "esp32dev".

Flash size settings are already configured in the following files.

| filename | flash size | app part size | data part size |
|----------|------------|---------------|----------------|
| `partition_noOta_1Mapp_3Mspiffs.csv` | 4.0 MB | 1.2 MB | 2.8 MB |
| `partition_noOta_1Mapp_7Mspiffs.csv` | 8.0 MB | 1.2 MB | 6.8 MB |

No OTA partition, one app partition, one data partition as SPIFFS to store the images of the clock faces.

These files are used by PlatformIO to create the partions on the flash of the ESP32 when uploading.

Upload port is set to 921600 baud in the `platformio.ini` file.
**Note**: Some clocks do not support such high speed, if you have issues, reduce this to 512000 baud or even lower.

#### Libraries in use

All the listed libraries are in use. The most recent versions are automatically installed from the [PlatformIO registry](https://registry.platformio.org) (or the given source location) as soon as the project is opened or before first compilation. 

It will take a while to initally install them - observe status messages in build log screen.

Compiles and works fine with listed library versions below, as of 2024-02-03. Newer (or possibly older) versions should be fine too.

If you have issues with automatic installation, here are locations of the original source code.

| Library | Author | Version | Link |
| ------ | ------ | ------ | ------ |
| NTPClient | Fabrice Weinberg | 3.2.1 | https://github.com/arduino-libraries/NTPClient |
| Adafruit NeoPixel | Adafruit | 1.12.0 |  https://github.com/adafruit/Adafruit_NeoPixel |
| DS1307RTC | Paul Stoffregen | 1.4.1 | https://github.com/PaulStoffregen/DS1307RTC |
| Time | Paul Stoffregen  | 1.6.1 | https://github.com/PaulStoffregen/Time |
| TFT_eSPI | Bodmer | 2.5.34 | https://github.com/Bodmer/TFT_eSPI |
| PubSubClient | Nick O'Leary  | 2.8.0 | https://www.arduinolibraries.info/libraries/pub-sub-client |
| ArduinoJson | Benoit Blanchon  | 7.0.2 | https://github.com/bblanchon/ArduinoJson.git |
| RTC by Makuna | Michael C.Miller  | 2.4.2 | https://github.com/Makuna/Rtc/wiki |
| SparkFun APDS9960 RGB and Gesture Sensor | SparkFun  | 1.4.3 | https://github.com/sparkfun/SparkFun_APDS-9960_Sensor_Arduino_Library |

**Notes**:
`RTC by Makuna` is only required for "SI HAI clock".

`sparkfun/SparkFun APDS9960 RGB and Gesture Sensor` is only required by NovelLife SE clocks with gesture sensor.

`IPgeolocation` and `NTPclient` libraries were copied into the project and heavily updated (mostly bug fixes and error-catching).

#### Configure the `TFT_eSPI` library

The supplied `script_configure_tft_lib.py` takes care for library configuration. It copies two files (`_USER_DEFINES.h` and `GLOBAL_DEFINES.h`) into the library folder before building. This makes sure, the TFT_eSPI library is initalized with the correct values for each clock type.

If you have issues with the scripts, copy the files manually every time the `TFT_eSPI` library is updated.

Note: For IPSTUBE clocks use "esp32dev8MB" environment, then `script_configure_tft_lib_8MB.py` is used.

#### Configure the `APDS9960` library (NovelLife SE only)

The supplied `script_adjust_gesture_sensor_lib.py` modifies some files of the APDS9960 library  before building. It adds the support for the ID of the used (cloned) gesture chip.

Note: For IPSTUBE clocks use "esp32dev8MB" environment, then `script_adjust_gesture_sensor_lib_8MB.py` is used.

### Configure, Build and Upload New Firmware

Make sure you configured everything in `_USER_DEFINES.h`:

- Rename/Copy `_USER_DEFINES - empty.h` to `_USER_DEFINES.h`
- Select the target hardware platform (Elekstube, NovelLife, SI_HAI, PunkCyber/RGB Glow tube, IPSTUBE) by uncommenting the appropriate hardware define (only ONE clock type can be defined at a time, so comment out the unwanted)
- Select if you prefer WPS or hardcoded credentials for WiFi (comment '#define WIFI_USE_WPS' line and enter your WiFi network credentials to the other lines and uncomment them)
- Select image type for the clock faces to store: Bitmap files (.BMP) or .CLK files (uncomment #define USE_CLK_FILES)

Optionally:

- Enable integrated MQTT service (uncomment '#define MQTT_ENABLED' line and enter your MQTT credentials. From your local broker or from an internet-based broker.
E.g. register on [SmartNest.cz](https://www.smartnest.cz/), create a Thermostat device, copy your username, API key and Thermostat Device ID.
- Use IP-based geolocation by Abstract (uncomment #define GEOLOCATION_ENABLED and enter your geolocation API key: Register on [Abstract API](https://www.abstractapi.com/), select Geolocation API and copy your API key.
- Enable temperature sensor (uncomment and define pin for external DS18B20 temperature sensor) - not available for most of the clocks.

Connect the clock to your computer via a USB cable. You'll see, that a new serial port is detected and showing up in the device configuration. If not, check the section "Install the USB Serial Port Device Driver".

PlatformIO will automatically select the right port for uploading (in most cases).

Most clocks will go into to the download mode automatically, when PlatformIO is trying to upload the builded firmware files.
Some clocks needs a button pressed while the powering phase (plugging the USB cable), like the IPSTUBEs.

 **Note**: If you have Bluetooth virtual ports on your machine, it might hang and you must manually select the COM port in the `platformio.ini`, see [Upload options](https://docs.platformio.org/en/latest/projectconf/sections/env/options/upload/index.html).

#### 1) Compile the code and upload the firmware file (to the app partition)

Compile the code via the "Build" command of PlatformIO extension and upload the code via the "Upload" command in the matching environment for your clock (esp32dev is right for all clocks, except from IPSTUBE)

![alt text](/documentation/ImagesMD/PlatformIOBuild.png)

At this point, it should build cleanly and upload successfully.

Building:
![alt text](/documentation/ImagesMD/PlatformIOBuildOutput.png)

Uploading:
![alt text](/documentation/ImagesMD/PlatformIOUploadOutput.png)

Note: For IPSTUBE clocks use "esp32dev8MB" environment

**Note**: On auto-reset clocks, you'll see the clock boot up after upload. It will go into the setup routine and ask for WPS (if configured in the `_USER_DEFINES.H`) or connecting to the configrued Wifi, but it doesn't have any bitmaps to display on the screen yet!

The screens will stay blank until you upload data!

#### 2) Fill data partition (SPIFFS) with images

The repository comes with a "predefined" set of BMP files in the `data` subdirectory of the `EleksTubeHAX_pio` folder.

Each set of 10 images (one image for each digit) is called a "clock face" and can be chosen from the clock menu. Each set represents normally a different design or 'fonts' for the clock.
See below if you want to make your own.

Note: If MQTT is enabled clock face switching can also be done via the MQTT "set temperature" topic.

Note: All files in the `data` directory will be packed into the SPIFFS flash image! Make sure to tidy it up regulary.

##### Generate and upload

In PlatformIO extension go to "Project Tasks" and expand: esp32dev -> Platform
Select "Build Filesystem Image" first, then connect the clock and click "Upload Filesystem Image".

![alt text](/documentation/ImagesMD/PlatformIOBuildFilesystem.png)

This will upload the files to the SPIFFS filesystem on the ESP32 (flash of the clock).
They'll stay there, even if you re-upload the firmware to the app partition, because the data partition is not overriden or modified by that.

Note: For IPSTUBE clocks use "esp32dev8MB" environment

#### Custom clock faces from Bitmaps

If you want to change the uploaded clock faces for the clock:

- Create your own set of BMP files or select and copy some from the provided sets in the `data - other graphics` folder of this repo or download some from the internet (see below).
- Max resolution of each image is 135 x 240 pixels (HxW). They can be smaller, then the picture will be centered on the display.
- Max color depth is 24 bit RGB. But recommended is paletized Bitmaps with 256 colors palette.
- Name them `10.bmp` (for digit Zero) through `19.bmp` (for digit Nine); `20.bmp` to `29.bmp`, and so on. Note: There is no set 00-09.bmp!
- You can add max 8 clock face sets in the moment (Due to a problem in the detection meachanism - will be fixed soon).

Tips:

- Cut away any black border, this only eats away valuable flash storage space!
- Run your preferred image editor and play with reduced bit depths / paletization of the image!
- Very good results are with Dithering and 256-color palette. Size reduction is approx 70%.
- With very simple images (like 7-segment digits) even 16-color palette is enough and reduces size even further.

#### CLK files as alternative

Before supporting paletized Bitmaps, there was a special format used to store images, to safe some space on the flash of the clock. You can still switch to use CLK (Clock) files only, but there is no space saving anymore, if palatized Bitmaps are used!

To convert existing image files to CLK format:

- Run the tool `\Prepare_images\Convert_BMP_to_CLK.exe` (Windows only)
- You can select all BMP files to be converted at once. It will create CLK files from it.
- For a 24 bit depth Bitmap, size reduction is approx 30%.
- If needed, rename the generated files and put them in the `data` directory.
- Then do the "Build Filesystem Image & Upload Filesystem Image" dance again.

Note: It is either Bitmap or CLK! No mixing, so make sure to "clean" the `data` folder before switching.

#### Clock faces

Here are links to some good 3rd party sets out there:

- <https://github.com/upiir/ips_clock_100x_themes>
- <https://github.com/upiir/elekstube_ips_custom_theme>
- <https://github.com/upiir/rgb_glow_tube_clock>

If you have your own clock face that'll work and want it listed here, please file an Issue and/or Pull Request.

#### Configure your WiFi network

- For WPS: When prompted by the clock, press WPS button on your router (or in web-interface of your router). Clock will automatically connect to the WiFi and save data for future use. No need to input your credentials anywhere in the source code. The clock will remember WiFi connection details even if you unplug the clock.
- Without WPS: Add your WiFi credentials into `_USER_DEFINES.h` file before building the firmware.

## Development Process/History

See [Old Readme File](/README_OLD.md) for details.

[Reddit discussion on the original hack is here.](https://www.reddit.com/r/arduino/comments/mq5td9/hacking_the_elekstube_ips_clock_anyone_tried_it/)

[Original documentation and software from EleksMaker.](https://wiki.eleksmaker.com/doku.php?id=ips)

For Elekstube OV and SI HAI:
Hardware pinout and notes are in the document `Hardware pinout.xlsx` in the `documentation` folder

There was an offical "manual" and software package avaiable for the original Elekstube clock.
The website is not maintained anymore, so just for archive purpose:
See [EleksTube instructions](https://wiki.eleksmaker.com/doku.php?id=ips)

### Hardware modification

The "Original Version" of the EleksTube clock has a few problems in the hardware design. Most notably it forces 5V signals into ESP32 which is not happy about it. And it is outside of safe operating limits. This significantly reduces the lifetime of the ESP322. Mine (orignal Author) died because of this...

_Conversion:_
The CH340 chip, used for USB-UART conversion, can operate both on 5V and 3.3V. On the board it is powered by 5V. Cut one trace on the bottom side of the board that supplies the chip with 5V and route the 3.3V over the resistors / capacitors to VDD and VREF.

![Hardware modification](/documentation/ImagesMD/EleksTube_IPS_CH340C_mod.jpg)

## Main Contributors

- Mark Smith, aka Smitty ... @SmittyHalibut on GitHub, Twitter, and YouTube.
- Aljaz Ogrin, aka aly-fly ... @aly-fly on GitHub and Instagram
- Misc code snips either commited by or copied from: @icebreaker-ch, @meddle99, @OggyP, @bitrot-alpha
- in future (on to-do list) also from: @RedNax67, @wfdudley, @judge2005

_Happy hacking!_
