# HowAILens V0

HowAILens V0 is a Seeed Studio XIAO ESP32S3 Sense prototype for testing the
first wearable AI loop:

```text
touch tap -> camera capture -> OLED status -> browser can view JPEG
```

The current firmware keeps the proven Arduino IDE behavior, but is now set up as
a PlatformIO project.

The long-term goal is to integrate this wearable capture/display prototype with
HowAI, the broader AI application this project is named for.

V0.5 adds a local backend upload test:

```text
touch tap -> camera capture -> POST JPEG to Mac -> OLED shows backend response
```

V0.5.1 hardens that loop with backend health checks, clearer OLED states, Wi-Fi
reconnect handling, and a simple backend browser page.

V0.6 adds AI vision in the backend:

```text
touch tap -> POST JPEG to Mac -> OpenAI vision -> short OLED-safe response
```

V0.7 shapes AI responses for the tiny OLED:

```text
max 3 lines -> max 12 chars per line
```

## Parts Used

This is currently a desk prototype, not a polished wearable build.

Main electronics:

- Seeed Studio XIAO ESP32S3 Sense
- Built-in XIAO camera
- Built-in XIAO microphone, not used yet
- XIAO Wi-Fi/BLE
- XIAO PSRAM for camera frame buffers
- MusRock 0.66 inch 64x48 SSD1306 OLED display module
- TTP223 capacitive touch sensor

Current power and wiring:

- USB-C cable from MacBook to XIAO for development power and upload
- Dupont/jumper wires for the desk prototype
- OLED and touch sensor share XIAO `3V3` and `GND`
- OLED uses I2C in this build
- No separate switch or power-management parts yet

OLED wiring:

```text
OLED GND -> XIAO GND
OLED VCC -> XIAO 3V3
OLED SDA -> XIAO D4
OLED SCL -> XIAO D5
```

Touch sensor wiring:

```text
TTP223 VCC     -> XIAO 3V3
TTP223 GND     -> XIAO GND
TTP223 OUT/SIG -> XIAO D2
```

The OLED listing is for a 0.66 inch 64x48 white/blue SSD1306 module with
IIC/I2C/SPI in the product title, but this project currently uses the 4-pin I2C
interface. Amazon listing/ASIN: `B0FGD4D4MS`.

Planned but not part of the current build:

- 3.7V LiPo battery
- Physical power switch
- Dedicated power-management parts
- Wearable frame mounting
- Short soldered silicone wiring

Current wiring is documented in
`docs/haolens-v0-project-brief.md`.

OLED note: the display is tiny. In the current firmware/backend contract, assume
up to 11 short characters per line and design status/AI text as short phrases,
not paragraphs.

Firmware OLED limits live in `platformio.ini` build flags. Backend AI text
shaping uses `.env`. Keep both set to the same line shape for this display.

## Configure Wi-Fi

Edit `include/secrets.h` before uploading:

```cpp
#define HAOLENS_WIFI_SSID "YOUR_WIFI_NAME"
#define HAOLENS_WIFI_PASS "YOUR_WIFI_PASSWORD"
#define HAOLENS_BACKEND_URL "http://YOUR_MAC_IP:8787/analyze"
```

`include/secrets.h` is ignored by git. Use `include/secrets.example.h` as the
shareable template.

The backend URL must use your Mac's LAN IP address, not `localhost`, because the
XIAO is a separate device on Wi-Fi.

## Local Backend

Use the project Node version:

```sh
nvm use
```

Configure the local backend:

```sh
cp .env.example .env
```

For AI vision, edit `.env` and set:

```text
OPENAI_API_KEY=...
OPENAI_MODEL=gpt-5.4-mini
OPENAI_IMAGE_DETAIL=low
HAOLENS_OLED_LINE_CHARS=11
HAOLENS_OLED_MAX_LINES=3
```

These backend OLED values should match the firmware build flags in
`platformio.ini`.

If `OPENAI_API_KEY` is empty, the backend stays in mock mode and returns the
configured `HAOLENS_AI_MOCK_TEXT`.

Start the local backend:

```sh
node tools/local-backend/server.js
```

The server prints usable URLs such as:

```text
http://192.168.50.179:8787/analyze
```

Copy the Wi-Fi/LAN address into `HAOLENS_BACKEND_URL`, upload the firmware, then
tap the touch sensor. The backend saves the most recent uploaded image here:

```text
tools/local-backend/uploads/latest.jpg
```

You can also view it in a browser:

```text
http://<YOUR_MAC_IP>:8787/
http://<YOUR_MAC_IP>:8787/latest.jpg
http://<YOUR_MAC_IP>:8787/health
http://<YOUR_MAC_IP>:8787/last-analysis
```

Firmware checks `/health` on boot and before upload if the backend was not
already marked available.

`POST /analyze` intentionally returns compact JSON so the ESP32 can parse it
with a small memory budget. The firmware reads the `text` field:

```json
{ "ok": true, "text": "short OLED result", "mode": "openai" }
```

For V0.7, the backend asks the model for up to three OLED-ready lines and still
sanitizes the result before sending it to the board. Example:

```text
Cable
near wall
```

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

For start/stop/restart commands for the backend and XIAO frontend, see
[docs/howailens-service-ops.md](docs/howailens-service-ops.md).

## Browser Routes

After upload, watch Serial Monitor for the board IP address, then open:

```text
http://<XIAO_IP>/
http://<XIAO_IP>/capture
http://<XIAO_IP>/last
```

Tap the touch sensor first, then open `/last` to view the latest touch-triggered
photo. Open `/` to see firmware status, backend status, last capture size, and
the last AI result.
