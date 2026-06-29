#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== Wi-Fi =====
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ===== OLED =====
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== Touch sensor =====
// TTP223 OUT/SIG -> XIAO D2
#define TOUCH_PIN D2

// ===== XIAO ESP32S3 Sense camera pins =====
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15

#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

WebServer server(80);

// ===== Latest touch-captured image stored in PSRAM =====
uint8_t* lastJpeg = nullptr;
size_t lastJpegLen = 0;

// ===== Touch debounce =====
bool lastTouched = false;
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 800;

// ===== OLED helper =====
void showMessage(const char* line1, const char* line2 = "", const char* line3 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println(line1);
  if (strlen(line2) > 0) display.println(line2);
  if (strlen(line3) > 0) display.println(line3);

  display.display();
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

  // You said XGA worked, so keeping it.
  // If it gets unstable, change this to FRAMESIZE_VGA or FRAMESIZE_QVGA.
  config.frame_size = FRAMESIZE_XGA;   // 1024x768
  config.jpeg_quality = 10;            // lower = better quality, bigger file
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

  Serial.printf("Browser captured JPEG size: %u bytes\n", fb->len);

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
  showMessage("Capturing...", "Touch trigger");

  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed.");
    showMessage("Capture", "failed");
    return false;
  }

  Serial.printf("Touch captured JPEG size: %u bytes\n", fb->len);

  // Free previous stored image
  if (lastJpeg != nullptr) {
    free(lastJpeg);
    lastJpeg = nullptr;
    lastJpegLen = 0;
  }

  // Allocate PSRAM for the latest image
  lastJpeg = (uint8_t*)ps_malloc(fb->len);

  if (lastJpeg == nullptr) {
    Serial.println("Failed to allocate memory for latest JPEG.");
    showMessage("Memory", "failed");
    esp_camera_fb_return(fb);
    return false;
  }

  memcpy(lastJpeg, fb->buf, fb->len);
  lastJpegLen = fb->len;

  esp_camera_fb_return(fb);

  char sizeText[24];
  snprintf(sizeText, sizeof(sizeText), "%u bytes", lastJpegLen);

  showMessage("Saved photo", sizeText, "Open /last");

  Serial.println("Touch photo saved. Open /last to view it.");
  return true;
}

// ===== View last touch-triggered capture =====
void handleLast() {
  if (lastJpeg == nullptr || lastJpegLen == 0) {
    server.send(404, "text/plain", "No touch-captured image yet. Touch the sensor first.");
    return;
  }

  Serial.printf("Serving last touch image: %u bytes\n", lastJpegLen);

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

  showMessage("WiFi", "Connecting...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("Open this URL: http://");
  Serial.println(WiFi.localIP());

  String ip = WiFi.localIP().toString();
  showMessage("Open URL", ip.c_str());

  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/last", handleLast);
  server.begin();

  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();

  bool touched = digitalRead(TOUCH_PIN) == HIGH;
  unsigned long now = millis();

  if (touched && !lastTouched && now - lastTouchTime > TOUCH_DEBOUNCE_MS) {
    lastTouchTime = now;
    Serial.println("Touch trigger detected.");
    captureAndStoreLatest();
  }

  lastTouched = touched;
}