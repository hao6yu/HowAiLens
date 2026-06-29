# HaoLens V0

HaoLens V0 is a Seeed Studio XIAO ESP32S3 Sense prototype for testing the first
AI glasses loop:

```text
touch tap -> camera capture -> OLED status -> browser can view JPEG
```

The current firmware keeps the proven Arduino IDE behavior, but is now set up as
a PlatformIO project.

## Hardware

- Seeed Studio XIAO ESP32S3 Sense
- MusRock 0.66 inch 64x48 SSD1306 OLED on I2C
- TTP223 capacitive touch sensor
- USB-C power during development
- No separate switch or power-management parts yet

Current wiring is documented in
`docs/haolens-v0-project-brief.md`.

OLED note: the display is tiny. At the current default font size, assume roughly
10 characters per line and design status/AI text as short phrases, not
paragraphs.

## Configure Wi-Fi

Edit `include/secrets.h` before uploading:

```cpp
#define HAOLENS_WIFI_SSID "YOUR_WIFI_NAME"
#define HAOLENS_WIFI_PASS "YOUR_WIFI_PASSWORD"
```

`include/secrets.h` is ignored by git. Use `include/secrets.example.h` as the
shareable template.

## PlatformIO Workflow

From VS Code, use the PlatformIO sidebar:

- Build: checkmark icon
- Upload: right-arrow icon
- Serial Monitor: plug icon

From a terminal, this machine currently has PlatformIO here:

```sh
/Users/haoyu/.platformio/penv/bin/pio run
/Users/haoyu/.platformio/penv/bin/pio run -t upload
/Users/haoyu/.platformio/penv/bin/pio device monitor
```

Serial monitor speed is `115200`.

## Browser Routes

After upload, watch Serial Monitor for the board IP address, then open:

```text
http://<XIAO_IP>/
http://<XIAO_IP>/capture
http://<XIAO_IP>/last
```

Tap the touch sensor first, then open `/last` to view the latest touch-triggered
photo.
