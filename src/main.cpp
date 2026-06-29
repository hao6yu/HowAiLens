#include <Arduino.h>
#include "esp_camera.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <cstring>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef HAOLENS_WIFI_SSID
#define HAOLENS_WIFI_SSID "YOUR_WIFI_NAME"
#endif

#ifndef HAOLENS_WIFI_PASS
#define HAOLENS_WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif

#ifndef HAOLENS_BACKEND_URL
#define HAOLENS_BACKEND_URL ""
#endif

// ===== Wi-Fi =====
const char* WIFI_SSID = HAOLENS_WIFI_SSID;
const char* WIFI_PASS = HAOLENS_WIFI_PASS;
const char* BACKEND_URL = HAOLENS_BACKEND_URL;

// ===== OLED =====
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

constexpr uint8_t OLED_CHARS_PER_LINE = 10;
constexpr uint8_t OLED_MAX_LINES = 6;

// ===== Touch sensor =====
// TTP223 OUT/SIG -> XIAO D2
#define TOUCH_PIN D2

// ===== XIAO ESP32S3 Sense camera pins =====
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 10
#define SIOD_GPIO_NUM 40
#define SIOC_GPIO_NUM 39

#define Y9_GPIO_NUM 48
#define Y8_GPIO_NUM 11
#define Y7_GPIO_NUM 12
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 16
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 17
#define Y2_GPIO_NUM 15

#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM 47
#define PCLK_GPIO_NUM 13

WebServer server(80);

// ===== Latest touch-captured image stored in PSRAM =====
uint8_t* lastJpeg = nullptr;
size_t lastJpegLen = 0;

// ===== Touch debounce =====
bool lastTouched = false;
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 800;

// ===== Connection state =====
bool wifiWasConnected = false;
bool backendAvailable = false;
unsigned long lastWifiReconnectAttempt = 0;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;
const unsigned long BACKEND_HEALTH_TIMEOUT_MS = 5000;
const unsigned long BACKEND_UPLOAD_TIMEOUT_MS = 15000;

bool backendIsConfigured();

// ===== OLED helper =====
String fitOledLine(String line) {
  line.trim();

  if (line.length() <= OLED_CHARS_PER_LINE) {
    return line;
  }

  return line.substring(0, OLED_CHARS_PER_LINE - 3) + "...";
}

String addOledEllipsis(String line) {
  line.trim();

  if (line.length() <= OLED_CHARS_PER_LINE - 3) {
    return line + "...";
  }

  return line.substring(0, OLED_CHARS_PER_LINE - 3) + "...";
}

void drawOledLines(String lines[], uint8_t lineCount) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  for (uint8_t i = 0; i < lineCount && i < OLED_MAX_LINES; i++) {
    display.setCursor(0, i * 8);
    display.println(fitOledLine(lines[i]));
  }

  display.display();
}

void showMessage(const char* line1, const char* line2 = "", const char* line3 = "") {
  String lines[OLED_MAX_LINES];
  uint8_t lineCount = 0;

  if (std::strlen(line1) > 0) lines[lineCount++] = line1;
  if (std::strlen(line2) > 0) lines[lineCount++] = line2;
  if (std::strlen(line3) > 0) lines[lineCount++] = line3;

  drawOledLines(lines, lineCount);
}

void showWrappedText(const String& text, uint8_t maxLines = OLED_MAX_LINES) {
  String lines[OLED_MAX_LINES];
  uint8_t lineCount = 0;
  int position = 0;

  while (position < static_cast<int>(text.length()) && lineCount < maxLines) {
    while (position < static_cast<int>(text.length()) &&
           (text[position] == ' ' || text[position] == '\r' || text[position] == '\n')) {
      position++;
    }

    if (position >= static_cast<int>(text.length())) {
      break;
    }

    int lineStart = position;
    int lineEnd = min(lineStart + OLED_CHARS_PER_LINE, static_cast<int>(text.length()));
    int newlineAt = text.indexOf('\n', lineStart);

    if (newlineAt >= lineStart && newlineAt < lineEnd) {
      lineEnd = newlineAt;
    } else if (lineEnd < static_cast<int>(text.length()) && text[lineEnd] != ' ') {
      for (int i = lineEnd - 1; i > lineStart + 2; i--) {
        if (text[i] == ' ') {
          lineEnd = i;
          break;
        }
      }
    }

    lines[lineCount++] = text.substring(lineStart, lineEnd);
    position = lineEnd;
  }

  if (position < static_cast<int>(text.length()) && lineCount > 0) {
    lines[lineCount - 1] = addOledEllipsis(lines[lineCount - 1]);
  }

  drawOledLines(lines, lineCount);
}

// ===== Camera init =====
bool initCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // XGA worked in the Arduino sketch. If it gets unstable, try VGA or QVGA.
  config.frame_size = FRAMESIZE_XGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  Serial.println("Camera initialized.");
  return true;
}

// ===== HTML menu page =====
void handleRoot() {
  String html = "";
  html += "<html><body>";
  html += "<h2>XIAO AI Glass Camera</h2>";
  html += "<p><a href='/capture'>Browser Capture</a></p>";
  html += "<p><a href='/last'>View Last Touch Capture</a></p>";
  html += "<p>Tap the touch sensor, then open /last.</p>";
  html += "<p>V0.5.1: touch capture also posts to the local backend if configured.</p>";
  html += "<p>Backend: ";
  html += backendIsConfigured() ? (backendAvailable ? "OK" : "not reachable") : "off";
  html += "</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// ===== Browser-triggered fresh capture =====
void handleCapture() {
  Serial.println("Browser requested capture...");
  showMessage("Browser", "Capturing...");

  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed.");
    showMessage("Capture", "failed");
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  Serial.printf("Browser captured JPEG size: %u bytes\n", static_cast<unsigned>(fb->len));

  WiFiClient client = server.client();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/jpeg");
  client.println("Cache-Control: no-store");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(fb->len);
  client.println();

  client.write(fb->buf, fb->len);

  esp_camera_fb_return(fb);

  showMessage("Browser", "Captured");
}

// ===== Touch-triggered capture stored in memory =====
bool captureAndStoreLatest() {
  Serial.println("Touch capture requested...");
  showMessage("Capturing", "Touch");

  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed.");
    showMessage("Capture", "failed");
    return false;
  }

  Serial.printf("Touch captured JPEG size: %u bytes\n", static_cast<unsigned>(fb->len));

  if (lastJpeg != nullptr) {
    free(lastJpeg);
    lastJpeg = nullptr;
    lastJpegLen = 0;
  }

  lastJpeg = static_cast<uint8_t*>(ps_malloc(fb->len));

  if (lastJpeg == nullptr) {
    Serial.println("Failed to allocate memory for latest JPEG.");
    showMessage("Memory", "failed");
    esp_camera_fb_return(fb);
    return false;
  }

  std::memcpy(lastJpeg, fb->buf, fb->len);
  lastJpegLen = fb->len;

  esp_camera_fb_return(fb);

  char sizeText[24];
  snprintf(sizeText, sizeof(sizeText), "%u bytes", static_cast<unsigned>(lastJpegLen));

  showMessage("Saved", sizeText, "Open /last");

  Serial.println("Touch photo saved. Open /last to view it.");
  return true;
}

bool backendIsConfigured() {
  return std::strlen(BACKEND_URL) > 0 && std::strstr(BACKEND_URL, "YOUR_MAC_IP") == nullptr;
}

String backendHealthUrl() {
  String url = BACKEND_URL;
  int schemeEnd = url.indexOf("://");
  int pathSearchStart = schemeEnd >= 0 ? schemeEnd + 3 : 0;
  int pathStart = url.indexOf('/', pathSearchStart);

  if (pathStart >= 0) {
    url = url.substring(0, pathStart);
  }

  return url + "/health";
}

void showReady() {
  if (!backendIsConfigured()) {
    showMessage("Ready", "Back off", "Tap photo");
    return;
  }

  showMessage("Ready", backendAvailable ? "Backend OK" : "Back fail", "Tap photo");
}

bool connectWiFi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.print("Connecting to WiFi");
  WiFi.disconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection timed out.");
    return false;
  }

  Serial.print("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool checkBackendHealth(bool updateDisplay = true) {
  if (!backendIsConfigured()) {
    Serial.println("Backend URL is not configured.");
    if (updateDisplay) showMessage("Backend", "off");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected; cannot check backend.");
    if (updateDisplay) showMessage("WiFi lost", "No check");
    return false;
  }

  String healthUrl = backendHealthUrl();
  Serial.print("Checking backend health: ");
  Serial.println(healthUrl);
  if (updateDisplay) showMessage("Backend", "checking");

  HTTPClient http;
  http.setTimeout(BACKEND_HEALTH_TIMEOUT_MS);

  if (!http.begin(healthUrl)) {
    Serial.println("Failed to start backend health request.");
    if (updateDisplay) showMessage("Backend", "setup fail");
    return false;
  }

  int statusCode = http.GET();
  String errorText = statusCode <= 0 ? http.errorToString(statusCode) : "";
  http.end();

  Serial.printf("Backend health status: %d\n", statusCode);

  if (statusCode >= 200 && statusCode < 300) {
    if (updateDisplay) showMessage("Backend OK");
    return true;
  }

  if (statusCode <= 0) {
    Serial.print("Backend health error: ");
    Serial.println(errorText);
    if (updateDisplay) showMessage("Backend", "offline");
    return false;
  }

  char statusText[16];
  snprintf(statusText, sizeof(statusText), "HTTP %d", statusCode);
  if (updateDisplay) showMessage("Backend", "failed", statusText);
  return false;
}

void ensureWiFiConnected() {
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      Serial.print("WiFi reconnected. IP: ");
      Serial.println(WiFi.localIP());
      backendAvailable = checkBackendHealth(false);
      showReady();
    }
    return;
  }

  if (wifiWasConnected) {
    Serial.println("WiFi disconnected.");
    showMessage("WiFi lost", "Retrying");
    wifiWasConnected = false;
    backendAvailable = false;
  }

  if (now - lastWifiReconnectAttempt < WIFI_RECONNECT_INTERVAL_MS) {
    return;
  }

  lastWifiReconnectAttempt = now;
  Serial.println("Attempting WiFi reconnect...");
  WiFi.disconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

String parseBackendText(const String& responseBody) {
  StaticJsonDocument<384> doc;
  DeserializationError error = deserializeJson(doc, responseBody);

  if (error) {
    Serial.print("Backend JSON parse failed: ");
    Serial.println(error.c_str());
    return "";
  }

  const char* text = doc["text"] | "";
  return String(text);
}

bool uploadLatestToBackend() {
  if (lastJpeg == nullptr || lastJpegLen == 0) {
    Serial.println("No stored JPEG available for backend upload.");
    showMessage("No photo", "to upload");
    return false;
  }

  if (!backendIsConfigured()) {
    Serial.println("Backend URL is not configured; skipping upload.");
    showMessage("Saved", "Back off");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected; cannot upload.");
    showMessage("WiFi lost", "Upload", "skip");
    return false;
  }

  if (!backendAvailable) {
    Serial.println("Backend is not marked available; checking before upload.");
    backendAvailable = checkBackendHealth(true);

    if (!backendAvailable) {
      showMessage("Backend", "offline");
      return false;
    }
  }

  Serial.print("Uploading latest image to backend: ");
  Serial.println(BACKEND_URL);
  showMessage("Uploading", "backend");

  HTTPClient http;
  http.setTimeout(BACKEND_UPLOAD_TIMEOUT_MS);

  if (!http.begin(BACKEND_URL)) {
    Serial.println("Failed to start HTTP connection.");
    showMessage("Upload", "setup fail");
    return false;
  }

  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("Cache-Control", "no-store");

  int statusCode = http.POST(lastJpeg, lastJpegLen);
  String responseBody = statusCode > 0 ? http.getString() : "";
  String errorText = statusCode <= 0 ? http.errorToString(statusCode) : "";
  http.end();

  Serial.printf("Backend HTTP status: %d\n", statusCode);
  Serial.print("Backend response: ");
  Serial.println(responseBody);

  if (statusCode <= 0) {
    Serial.print("Backend upload error: ");
    Serial.println(errorText);
    backendAvailable = false;
    showMessage("Upload", "failed", "No HTTP");
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    char statusText[16];
    snprintf(statusText, sizeof(statusText), "HTTP %d", statusCode);
    showMessage("Upload", "failed", statusText);
    return false;
  }

  String responseText = parseBackendText(responseBody);

  if (responseText.length() == 0) {
    responseText = "Image OK";
  }

  backendAvailable = true;
  showWrappedText(responseText, 4);
  return true;
}

// ===== View last touch-triggered capture =====
void handleLast() {
  if (lastJpeg == nullptr || lastJpegLen == 0) {
    server.send(404, "text/plain", "No touch-captured image yet. Touch the sensor first.");
    return;
  }

  Serial.printf("Serving last touch image: %u bytes\n", static_cast<unsigned>(lastJpegLen));

  WiFiClient client = server.client();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/jpeg");
  client.println("Cache-Control: no-store");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(lastJpegLen);
  client.println();

  client.write(lastJpeg, lastJpegLen);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting AI Glass camera web server...");

  pinMode(TOUCH_PIN, INPUT);

  Serial.printf("PSRAM found: %s\n", psramFound() ? "YES" : "NO");
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());

  // OLED I2C:
  // OLED SDA -> XIAO D4
  // OLED SCL -> XIAO D5
  Wire.begin(D4, D5);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found.");
    while (true) delay(1000);
  }

  showMessage("Starting", "Camera...");

  if (!initCamera()) {
    showMessage("Camera", "failed");
    while (true) delay(1000);
  }

  showMessage("WiFi", "Connect");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  while (!connectWiFi(WIFI_CONNECT_TIMEOUT_MS)) {
    showMessage("WiFi fail", "Retrying");
    delay(2000);
  }

  wifiWasConnected = true;
  Serial.print("Open this URL: http://");
  Serial.println(WiFi.localIP());
  Serial.print("Backend URL: ");
  Serial.println(backendIsConfigured() ? BACKEND_URL : "(not configured)");

  backendAvailable = checkBackendHealth(true);

  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/last", handleLast);
  server.begin();

  Serial.println("HTTP server started.");
  showReady();
}

void loop() {
  ensureWiFiConnected();
  server.handleClient();

  bool touched = digitalRead(TOUCH_PIN) == HIGH;
  unsigned long now = millis();

  if (touched && !lastTouched && now - lastTouchTime > TOUCH_DEBOUNCE_MS) {
    lastTouchTime = now;
    Serial.println("Touch trigger detected.");

    if (captureAndStoreLatest()) {
      uploadLatestToBackend();
    }
  }

  lastTouched = touched;
}
