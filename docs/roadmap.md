# HowAILens Roadmap

HowAILens is a proof-of-concept wearable AI client built around the Seeed XIAO
ESP32S3 Sense. The current prototype proves camera, Wi-Fi, OLED, and backend AI
integration. The longer-term goal is to connect the device to IMSAIGateway so
Ruby can act as a field audio agent for Archibus workflows.

## Product Direction

```text
HowAILens device
-> wearable mic/camera/status client

IMSAIGateway
-> Ruby realtime audio agent
-> Archibus tools
-> work request workflow
-> photo analysis

Browser portal
-> transcript
-> Ruby audio playback
-> work request/photo/tool timeline
```

The tiny OLED is not intended to be the main UI. It should remain a glanceable
status display for states such as `Listening`, `Thinking`, `Photo`, `WR ready`,
and `Error`.

## Current Status

V0 is working as a desk prototype.

Completed:

- PlatformIO project migration.
- XIAO ESP32S3 Sense camera capture.
- TTP223 touch-triggered capture.
- SSD1306 OLED status display.
- Wi-Fi connection and reconnect handling.
- XIAO-hosted debug page for last capture and status.
- Local Node backend for image uploads.
- Backend health checks.
- OpenAI vision integration through the backend.
- OLED-safe AI response shaping for the 64x48 display.
- Public repo docs for hardware, service ops, and future IMSAIGateway planning.
- IMSAIGateway live shell routes and mock-device proof point.
- Optional XIAO gateway WebSocket client for hello/status/OLED/photo commands.

## Near-Term Roadmap

### V0.7.x Stabilize Current Proof of Concept

Goal: keep the current camera/vision/OLED loop reliable enough for demos.

- Keep OLED text simple and readable.
- Keep local backend start/stop workflow documented.
- Keep PlatformIO build/upload workflow clean.
- Use the XIAO local page as a device/debug page.
- Avoid expanding the OLED into a full conversation display.

### V0.8 IMSAIGateway Realtime Client Foundation

Goal: prepare HowAILens to become an IMSAIGateway audio device after the
gateway Realtime v2 upgrade.

- Add a gateway-side HowAILens session shell. Done in IMSAIGateway commit
  `4ff4c95`.
- Use a mock device first, before hardware mic work.
- Separate the device audio connection from the browser portal connection.
- Let IMSAIGateway own the Ruby Realtime session.
- Let the portal show transcript and session state.
- Connect the XIAO firmware to the gateway shell for hello/status/OLED/photo
  command smoke testing.

Detailed plan:

- [HowAILens IMSAIGateway Realtime Plan](howailens-imsaigateway-realtime-plan.md)

### V0.9 XIAO Mic Streaming POC

Goal: prove the real hardware audio path.

- Capture microphone audio from the XIAO Sense.
- Stream PCM audio chunks over Wi-Fi to IMSAIGateway.
- Show live user transcript in the IMSAIGateway portal.
- Play Ruby audio through the browser portal.
- Keep device audio output out of scope.
- Keep OLED as status only.

Primary risk:

- audio format, sample rate, chunk timing, and mic noise.

### V0.10 Device Photo Tool

Goal: let Ruby request photos from the active HowAILens device.

- Gateway sends `capture_photo` command to the XIAO.
- XIAO captures JPEG and uploads it to IMSAIGateway.
- Gateway runs vision analysis.
- Gateway injects photo analysis into Ruby's active session.
- Portal shows the photo and analysis.

### V0.11 On-Device Audio Output POC

Goal: prove Ruby can speak back through the wearable device.

Planned parts:

- MAX98357A I2S amplifier
- Dayton BCE-1 bone conduction exciter

Scope:

- Add I2S audio output wiring and firmware support.
- Receive assistant audio from IMSAIGateway.
- Buffer and play short Ruby responses through the bone conduction exciter.
- Keep browser audio playback as the fallback/debug output.
- Test voice chat and live translation style interactions.

Primary risks:

- output volume and intelligibility
- power draw
- enclosure/mounting vibration
- audio buffering and latency
- microphone echo or feedback if listening while playing

## V1 Target Demo

Goal: field work request creation through Ruby.

Example:

```text
User:
Hey Ruby, we have a failed fire door. I want to submit a work request.

Ruby:
-> confirms user/location context
-> requests or confirms photo capture
-> analyzes photo
-> summarizes the issue
-> asks for explicit confirmation
-> creates the Archibus work request
-> shows WR number and photo in the portal
-> sends short status to OLED
```

Success means the wearable can act as a field input device for a real
IMSAIGateway/Archibus workflow.

## Later Work

Hardware:

- battery and power management
- physical switch
- wearable frame
- shorter soldered wiring
- MAX98357A and Dayton BCE-1 mounting/wiring refinement

Device UX:

- better button/touch gestures
- status LEDs or haptics if useful
- reconnect behavior
- device pairing

Agent/product:

- richer work request attachments
- better photo-to-WR context
- equipment/location recognition
- conversation history in IMSAIGateway
- supervisor/technician workflows

Production hardening:

- device authentication
- encrypted/public-network deployment model
- gateway authorization rules
- logging and diagnostics
- OTA update strategy
- enclosure and field testing

## Out of Scope for the Current POC

- production security
- always-on wake word
- on-device Ruby audio playback before the MAX98357A/BCE-1 output phase
- Bluetooth headset integration
- long-form text display on the OLED
- local AI inference on the ESP32S3
