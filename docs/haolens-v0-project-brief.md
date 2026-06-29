# HaoLens V0 — AI Glasses Prototype

## Project summary

**HaoLens V0** is an ugly-but-working AI glasses prototype built around the **Seeed Studio XIAO ESP32S3 Sense**. The goal is to prove the core smart-glasses loop first, before worrying about polish, custom PCB, optics, frame design, battery optimization, or audio.

Current V0 goal:

```text
Touch sensor tap
→ XIAO camera captures photo
→ OLED shows status
→ photo can be viewed from browser
→ next step: send photo to backend/AI
→ display short AI response on OLED
```

This project is intentionally starting small. It is not trying to build production smart glasses yet. The first milestone is proving the camera + input + display + Wi-Fi workflow.

Current V0.5 goal:

```text
Touch sensor tap
→ XIAO camera captures photo
→ latest photo is still available at /last
→ XIAO POSTs JPEG to local backend on Mac
→ backend saves latest.jpg
→ backend returns short JSON response
→ OLED displays backend response
```

V0.5 intentionally does not call AI yet. It proves the upload/response loop first.

---

## Current hardware

### Main board

- **Seeed Studio XIAO ESP32S3 Sense**
- Built-in camera
- Built-in microphone
- Wi-Fi / BLE
- USB-C programming and power
- LiPo battery charging support
- PSRAM available for camera frame buffer

### Display

- **MusRock 0.66 inch OLED display module**
- 64x48 resolution
- SSD1306 driver
- White/blue OLED listing
- I2C/IIC interface in current wiring
- Product listing also mentions SPI support, but the current module is used as a 4-pin I2C display:
  - `GND`
  - `VCC`
  - `SCL`
  - `SDA`
- 3.3V operation
- Amazon listing: `B0FGD4D4MS`

### Input

- **TTP223 capacitive touch sensor**
- Used as the first trigger button
- Touch = capture photo
- Pins: `VCC`, `GND`, `OUT/SIG`

### Power

- Currently powered by **USB-C from MacBook**
- Battery not required for coding/testing
- No separate switch or power-management parts have been purchased yet
- Battery will be added later for wearable testing
- Planned battery type:
  - 3.7V LiPo
  - JST 1.25mm connector
  - 200–300mAh preferred for wearable V1

### Mounting

- Current setup is desk prototype with jumper wires
- Future wearable V1 will use:
  - short silicone wires
  - Velcro / double-sided tape
  - cheap safety glasses frame
  - battery mounted along temple arm

---

## Current wiring

### OLED wiring

```text
OLED GND  → XIAO GND
OLED VCC  → XIAO 3V3
OLED SDA  → XIAO D4
OLED SCL  → XIAO D5
```

### Touch sensor wiring

```text
TTP223 VCC      → XIAO 3V3
TTP223 GND      → XIAO GND
TTP223 OUT/SIG  → XIAO D2
```

### Shared power note

Both OLED and touch sensor use the same `3V3` and `GND`.

For desk testing, this may be done with Dupont wires / temporary split wiring.

For wearable V1, replace temporary wires with a soldered Y-split:

```text
XIAO 3V3
  ├── OLED VCC
  └── Touch VCC

XIAO GND
  ├── OLED GND
  └── Touch GND
```

---

## Arduino IDE setup used so far

Board selected:

```text
XIAO_ESP32S3
```

Port example:

```text
/dev/cu.usbmodem101
```

Libraries installed:

```text
Adafruit SSD1306
Adafruit GFX Library
```

Serial monitor baud rate:

```text
115200
```

Important note:

- `XIAO_ESP32S3` board selection works.
- Camera started working correctly after the right board/PSRAM configuration was available.
- If camera frame buffer allocation fails, check PSRAM settings/board package.

---

## Working milestones completed

### 1. XIAO serial upload test

Confirmed:

```text
MacBook → USB-C → Arduino IDE → XIAO
```

The board successfully uploads code and prints to Serial Monitor.

### 2. OLED test

Confirmed OLED works over I2C.

Serial output example:

```text
Starting OLED test...
OLED found!
Text sent to OLED.
```

OLED displayed text like:

```text
Hello
XIAO
AI Glass
```

### 3. Touch sensor test

Confirmed TTP223 touch sensor works.

Serial output example:

```text
Starting OLED + Touch test...
Ready. Touch the sensor.
Touched!
Released.
Touched!
Released.
```

OLED changes when touch sensor is tapped.

### 4. Camera capture test

Confirmed touch can trigger camera capture.

Serial output example:

```text
Starting AI Glass camera trigger test...
Camera initialized.
Touch trigger detected.
Capturing photo...
Photo captured. Size: 4136 bytes
```

This proves:

```text
Touch sensor → XIAO → camera capture
```

### 5. Browser image capture test

Confirmed XIAO can host a local web server over Wi-Fi.

Browser route:

```text
http://<XIAO_IP>/capture
```

This returns a JPEG captured by the XIAO camera.

### 6. Touch-triggered latest image

Confirmed touch sensor can capture and store a latest image in PSRAM.

Routes now:

```text
/          = menu page
/capture   = browser-triggered fresh capture
/last      = view last touch-triggered photo
```

Current behavior:

```text
Touch sensor tap
→ OLED shows capture status
→ XIAO captures photo
→ image stored as latest JPEG
→ browser can open /last to view it
```

This is the current V0 working state.

---

## Current code behavior

The current firmware does the following:

1. Starts Serial Monitor at `115200`
2. Initializes OLED over I2C
3. Initializes XIAO camera
4. Connects to Wi-Fi
5. Starts HTTP server
6. Serves:
   - `/`
   - `/capture`
   - `/last`
7. Reads TTP223 touch sensor on `D2`
8. On touch:
   - captures photo
   - stores latest JPEG in PSRAM
   - updates OLED with image size
9. Browser can view latest touch-captured photo at `/last`

---

## Important security note

Do **not** commit real Wi-Fi password or API keys to GitHub.

Use placeholder values in code:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

Later, move secrets into one of these:

```text
include/secrets.h
.env
PlatformIO build flags
local config file ignored by Git
```

Add to `.gitignore` later:

```text
include/secrets.h
.env
```

---

## Recommended next step

The project should now move from Arduino IDE to:

```text
VS Code + PlatformIO
```

Reason:

Arduino IDE was great for hardware validation, but the project is about to grow into a real firmware codebase.

Next features will likely include:

```text
HTTP POST image upload
backend API integration
AI vision response
JSON parsing
OLED response rendering
Wi-Fi reconnect handling
battery status
config/secrets separation
multiple source files
```

PlatformIO will make this easier to maintain and easier for Codex to edit.

---

## Suggested PlatformIO project structure

```text
haolens-v0/
  platformio.ini
  README.md
  .gitignore
  include/
    config.example.h
    secrets.h              # local only, gitignored
  src/
    main.cpp
    display.cpp
    display.h
    camera_service.cpp
    camera_service.h
    touch_input.cpp
    touch_input.h
    web_server.cpp
    web_server.h
```

For the first PlatformIO migration, keep it simple:

```text
haolens-v0/
  platformio.ini
  src/
    main.cpp
```

Only split files after the current behavior works in PlatformIO.

---

## Suggested PlatformIO config

Start with this `platformio.ini`:

```ini
[env:seeed_xiao_esp32s3]
platform = espressif32
board = seeed_xiao_esp32s3
framework = arduino
monitor_speed = 115200

lib_deps =
  adafruit/Adafruit SSD1306
  adafruit/Adafruit GFX Library
```

If camera/PSRAM fails in PlatformIO, investigate board settings/build flags for PSRAM support.

---

## Migration plan from Arduino IDE to PlatformIO

### Step 1: Preserve working Arduino sketch

Save current working Arduino code as:

```text
ai_glasses_v0_touch_camera_web_working.ino
```

This is the known-good backup.

### Step 2: Create PlatformIO project

Create a new PlatformIO project:

```text
Project name: haolens-v0
Board: Seeed XIAO ESP32S3
Framework: Arduino
```

### Step 3: Copy Arduino code into `src/main.cpp`

The current `.ino` code should mostly work as C++.

If compilation fails due to function declarations, add forward declarations near the top, for example:

```cpp
void showMessage(const char* line1, const char* line2 = "", const char* line3 = "");
bool initCamera();
void handleRoot();
void handleCapture();
bool captureAndStoreLatest();
void handleLast();
```

### Step 4: Confirm same behavior

PlatformIO version must match Arduino behavior before adding new features:

```text
OLED works
Touch works
Camera works
Wi-Fi connects
/capture works
Touch capture works
/last works
```

### Step 5: Commit V0 baseline

Commit once PlatformIO version works:

```text
git commit -m "Initial HaoLens V0 touch camera web prototype"
```

---

## Next feature after PlatformIO migration

After the PlatformIO version is stable, build:

```text
Touch
→ capture photo
→ HTTP POST JPEG to backend
→ backend returns text
→ OLED displays returned text
```

This is the V0.5 milestone.

Recommended V1 backend path:

```text
Local Node.js server first
then Supabase Edge Function or hosted backend
```

Why local first:

```text
easier debugging
can save received image as latest.jpg
can inspect request body
can test without API costs first
```

First backend test should not call AI yet.

Expected first backend flow:

```text
XIAO POST /analyze with JPEG
Local server saves image
Local server returns: "Image OK"
XIAO OLED shows: "Image OK"
```

Then replace backend response with AI vision result.

Current local backend command:

```sh
node tools/local-backend/server.js
```

The firmware backend URL lives in local-only `include/secrets.h`:

```cpp
#define HAOLENS_BACKEND_URL "http://YOUR_MAC_IP:8787/analyze"
```

Use the Mac's LAN IP address printed by the backend. Do not use `localhost`,
because the XIAO runs on a separate device.

---

## Future AI flow

Final intended V1 AI loop:

```text
Touch sensor tap
→ XIAO captures JPEG
→ XIAO sends JPEG to backend
→ backend calls AI vision API
→ backend returns short text answer
→ XIAO displays answer on OLED
```

Example OLED responses:

```text
Person at desk
Invoice visible
Red car ahead
Text: Exit Only
```

Because the OLED is only 64x48, responses must be short.

---

## Known constraints

### OLED

- Very small screen
- Good for short status/result text only
- Not suitable for long AI responses
- At the current default font size, the display fits roughly 10 characters per line
- Although 64x48 pixels can physically fit about 6 tiny text rows, practical UI should use fewer rows for readability
- AI responses should be compressed to short phrases, ideally 1-3 lines
- Firmware should avoid sending long paragraphs directly to the OLED

### XIAO ESP32S3 Sense

- Good for capture/input/display
- Not ideal for local LLM/AI processing
- Wi-Fi image upload is better than BLE image transfer

### Battery

- USB-C is best for development
- Battery should only be used after code is stable
- Always check LiPo polarity with multimeter before plugging into XIAO

### Wearable wiring

- Breadboard is fine for desk testing
- Not suitable for wearable glasses
- Wearable V1 should use short soldered silicone wires and Y-split for shared `3V3`/`GND`

---

## Hardware debugging notes

If OLED stops working:

```text
Check GND
Check VCC -> 3V3
Check SDA -> D4
Check SCL -> D5
Try I2C address 0x3C then 0x3D
```

If touch stops working:

```text
Check OUT/SIG -> D2
Check VCC -> 3V3
Check GND -> GND
Serial Monitor should print touch events
```

If camera fails:

```text
Check camera ribbon cable
Check PSRAM enabled
Try smaller frame size:
  FRAMESIZE_VGA
  FRAMESIZE_QVGA
  FRAMESIZE_QQVGA
```

If browser capture fails:

```text
Confirm Mac and XIAO are on same Wi-Fi network
Use 2.4GHz Wi-Fi
Check Serial Monitor for IP address
Open http://<XIAO_IP>/
```

---

## Current recommended camera quality

Current code has tested larger image capture.

Suggested stable settings:

```cpp
config.frame_size = FRAMESIZE_VGA;
config.jpeg_quality = 10;
```

If stable, try:

```cpp
config.frame_size = FRAMESIZE_XGA;
config.jpeg_quality = 10;
```

If unstable or memory issues occur, step back to VGA.

---

## Project name ideas

Working project name:

```text
HaoLens
```

Suggested repo name:

```text
haolens-v0
```

Alternative names:

```text
OpenSight
TinyVision
SideEye AI
GhostLens
LensLab
```

---

## Current project status

Status:

```text
V0 hardware proof is working.
PlatformIO migration is working.
V0.5 local backend upload loop is working.
```

Completed:

```text
Serial upload
OLED display
Touch input
Camera capture
Wi-Fi web server
Browser capture
Touch-triggered latest capture
PlatformIO build/upload workflow
Public GitHub repo
V0.0 git tag
Local backend upload test
```

Next:

```text
Add AI vision response after upload loop is stable
Display AI result on OLED
Improve wearable packaging and power
```
