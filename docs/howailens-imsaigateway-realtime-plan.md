# HowAILens IMSAIGateway Realtime Plan

This plan tracks the HowAILens restart after the IMSAIGateway Realtime v2
upgrade.

## Current Integration Status

As of the IMSAIGateway `develop` commit `4ff4c95`, the gateway-side V0.8.0
shell exists:

```text
GET /api/v1/howailens/live
WS  /api/v1/howailens/device/ws
WS  /api/v1/howailens/portal/ws
GET /api/v1/howailens/state
POST /api/v1/howailens/photos/{request_id}
GET /api/v1/howailens/photos/{request_id}
```

The shell includes an in-memory session manager, browser portal page, mock
device script, raw image upload, and photo preview events. It does not yet
bridge real device audio into Ruby's Realtime session.

HowAILens firmware now has the V0.8.1 foundation:

- optional IMSAIGateway WebSocket client configuration
- `device_hello` on connect
- periodic `status` heartbeat
- remote `set_oled`
- remote `ping`
- remote `capture_photo` with JPEG upload to IMSAIGateway

The existing touch capture, local backend upload, and XIAO debug page remain
available.

## Goal

Use HowAILens as a wearable mic/camera client for the IMSAIGateway Ruby audio
agent.

```text
HowAILens XIAO
-> streams microphone audio over Wi-Fi
-> receives gateway commands
-> captures photos when Ruby asks
-> shows tiny OLED status only

IMSAIGateway
-> owns the Ruby OpenAI Realtime session
-> owns user/session context
-> executes Archibus and work request tools
-> analyzes photos
-> broadcasts transcript, audio, tool, and photo events to the portal

IMSAIGateway portal
-> shows realtime transcript
-> plays Ruby audio response
-> shows photos, tool calls, and work request results
```

The OLED is not the conversation UI. It should only show glanceable status such
as `Listening`, `Thinking`, `Photo`, `WR ready`, and `Error`.

## Target Demo

```text
User:
Hey Ruby, we have a failed fire door. I want to submit a work request.

Ruby:
1. Uses the authenticated user and default location from IMSAIGateway context.
2. Confirms or asks for the exact location if needed.
3. Commands HowAILens to capture a photo.
4. Receives the photo and runs gateway vision analysis.
5. Summarizes the issue and asks for explicit confirmation.
6. Calls the work request creation tool.
7. Shows the work request number, photo, and transcript in the portal.
8. Sends a short OLED status update to the device.
```

## Current Starting Point

HowAILens currently has:

- XIAO ESP32S3 Sense firmware in PlatformIO.
- Camera capture.
- OLED status/result display.
- Wi-Fi connection.
- Local backend upload loop.
- OpenAI vision proof of concept through the local backend.
- Optional IMSAIGateway device WebSocket connection.
- Remote gateway-driven OLED status updates.
- Remote gateway-driven photo capture/upload.

IMSAIGateway currently has useful pieces that should be reused after the
Realtime v2 upgrade:

- `app/endpoints/voice.py` for Ruby voice sessions.
- `app/voice_tools.py` for tool schemas and tool execution.
- `app/endpoints/work_requests.py` for work request functions.
- `app/endpoints/vision.py` for photo analysis and voice-session injection.

## Design Decision

Keep the main conversation screen in IMSAIGateway.

Use the XIAO local page only as a device/debug page:

- Wi-Fi status
- gateway connection status
- mic stream status
- last photo
- last error
- manual capture/test controls

This keeps identity, transcript, tool calls, audio playback, and work request UI
inside the gateway application where they belong.

## Proposed Gateway Shape

After the Realtime v2 upgrade, add a HowAILens adapter layer instead of forcing
the device into the existing browser voice endpoint.

```text
Device WebSocket
  XIAO -> gateway
  sends audio chunks, status, photo events
  receives commands

Portal WebSocket
  browser -> gateway
  receives transcript, assistant audio, tool events, photo previews
  may send operator/debug commands

Realtime Session
  gateway -> OpenAI
  receives device audio
  sends assistant audio/transcripts/tool calls
  executes gateway tools
```

The existing browser voice endpoint expects the same browser connection to send
audio and receive Ruby audio. HowAILens separates those roles, so a small
session manager should bridge device input, portal output, and the Realtime
session.

## Device WebSocket Draft

Device to gateway:

```json
{ "type": "device_hello", "device_id": "howailens-dev-01", "firmware": "0.8.x" }
{ "type": "audio", "seq": 1, "format": "pcm16", "sample_rate": 24000, "data": "base64..." }
{ "type": "status", "state": "listening", "wifi_rssi": -55 }
{ "type": "photo_ready", "request_id": "abc123", "upload_url": "/api/v1/howailens/photos/abc123" }
{ "type": "error", "message": "camera capture failed" }
```

Gateway to device:

```json
{ "type": "set_oled", "lines": ["Listening", "Ruby", "online"] }
{ "type": "capture_photo", "request_id": "abc123" }
{ "type": "start_audio" }
{ "type": "stop_audio" }
{ "type": "ping" }
```

Photo upload can be separate HTTP multipart upload if sending the JPEG over the
device WebSocket is too memory-heavy.

## Portal Event Draft

Gateway to portal:

```json
{ "type": "session_status", "state": "listening" }
{ "type": "transcript", "role": "user", "text": "we have a failed fire door" }
{ "type": "transcript", "role": "assistant", "text": "Got it, let me take a photo." }
{ "type": "assistant_audio", "format": "pcm16", "sample_rate": 24000, "data": "base64..." }
{ "type": "tool_call", "name": "capture_photo_from_device", "status": "started" }
{ "type": "photo", "url": "/api/v1/howailens/photos/abc123.jpg" }
{ "type": "wr_created", "wr_id": "12345" }
{ "type": "error", "message": "device disconnected" }
```

## Milestones

### V0.8.0 Gateway Shell

- Add a HowAILens Live page in IMSAIGateway.
- Add session manager skeleton for device, portal, and Realtime connections.
- Add mock device script that sends fake or silent PCM chunks.
- Show session status and mock transcript events in the portal.
- Status: implemented in IMSAIGateway `develop` commit `4ff4c95`.

Success:

```text
No hardware required. Portal can connect to a session and receive events.
```

### V0.8.1 Device Connection

- Add HowAILens device WebSocket client in firmware.
- Send `device_hello`, periodic `status`, and heartbeat messages.
- Receive `set_oled` and show status on OLED.
- Keep current camera and local debug page working.
- Status: firmware foundation implemented; hardware smoke test still needed.

Success:

```text
Gateway sees the device online. Gateway can update OLED status remotely.
```

### V0.8.2 Mic Streaming

- Capture XIAO Sense microphone audio.
- Convert or configure audio as gateway-compatible PCM16 mono.
- Stream audio chunks over the device WebSocket.
- Validate sample rate, chunk size, and timing.
- Show live transcript in the IMSAIGateway portal.

Success:

```text
Speaking near the XIAO produces transcript text in the portal.
```

### V0.8.3 Portal Audio Playback

- Relay Ruby assistant audio from Realtime to the portal.
- Play Ruby audio in browser.
- Show user transcript, assistant transcript, and status events.
- Keep device audio output out of scope until the MAX98357A/BCE-1 phase.

Success:

```text
XIAO is microphone input. Browser is Ruby audio output.
```

### Future Device Audio Output Phase

Planned hardware:

- MAX98357A I2S amplifier
- Dayton BCE-1 bone conduction exciter

This phase should happen after the device microphone stream and portal playback
are stable. It should prove Ruby audio playback from the device for live voice
chat or live translation style interactions.

### V0.8.4 Device Photo Tool

- Add gateway tool or command for `capture_photo_from_device`.
- Ruby can request a photo from the active HowAILens device.
- XIAO captures JPEG and uploads it to IMSAIGateway.
- Gateway runs photo analysis and injects the result into Ruby's active session.
- Portal shows the photo and analysis.

Success:

```text
Ruby asks for a photo, the device captures it, and Ruby can use the result.
```

### V0.8.5 Fire Door Work Request Demo

- Connect audio, photo, vision, confirmation, and work request creation.
- Use existing IMSAIGateway work request tools where possible.
- Keep explicit user confirmation before creating a work request.
- Show created work request result in the portal.
- Show `WR created` or short failure status on OLED.

Success:

```text
The fire door scenario works end to end in a controlled local demo.
```

## Engineering Risks

Audio transport is the highest-risk item.

Watch for:

- sample rate mismatch
- PCM format mismatch
- microphone noise
- chunk timing jitter
- WebSocket reconnect behavior
- ESP32 memory pressure
- camera capture interrupting audio streaming
- browser audio playback buffering

Acceptable for POC:

- touch/button/manual portal action instead of wake word
- browser audio output before the dedicated device audio-output phase
- brief audio pause during photo capture
- local LAN only
- simple device pairing or no auth

Out of scope for this phase:

- production security
- on-device wake word
- on-device speaker or bone-conduction audio before the MAX98357A/BCE-1 phase
- Bluetooth headset audio
- battery/power optimization
- polished wearable enclosure
- full Realtime v2 migration work inside IMSAIGateway

## Restart Checklist After IMSAIGateway Realtime v2 Upgrade

Before starting HowAILens V0.8 work, confirm:

- Ruby voice session works in IMSAIGateway after the v2 upgrade.
- Gateway can execute existing voice tools.
- Gateway can expose or add a session manager that accepts external audio input.
- Gateway can broadcast Realtime transcript and audio events to a portal client.
- Vision endpoint still supports image analysis.
- Vision analysis can still be injected into or attached to a live voice session.
- Work request creation flow still requires explicit confirmation.
- The target Realtime audio format is documented for the gateway adapter.

Then start with V0.8.0 using a mock device before touching firmware audio.

## First Implementation Task

The first IMSAIGateway-side skeleton is complete:

```text
/api/v1/howailens/device/ws
/api/v1/howailens/portal/ws
/api/v1/howailens/live
```

Next, hardware-smoke the XIAO gateway connection before bringing in microphone
complexity.
