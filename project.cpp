#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

// Replace with your WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Motion detection settings
#define MOTION_THRESHOLD 30
#define MOTION_CHECK_INTERVAL 500  // ms

// AI Thinker ESP32-CAM pins
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

WiFiServer server(80);

camera_fb_t *lastFrame = nullptr;
unsigned long lastMotionCheck = 0;
bool motionDetected = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Camera config
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;  // 800x600
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;   // 640x480
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  // OTA setup
  ArduinoOTA.onStart([]() {
    Serial.println("OTA update started");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update finished");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  server.begin();
  Serial.println("Server started");
}

// Simple motion detection by frame difference
bool detectMotion(camera_fb_t *currentFrame) {
  if (!lastFrame) {
    lastFrame = esp_camera_fb_get();
    return false;
  }

  int diffCount = 0;
  int thresholdDiff = 10;

  for (size_t i = 0; i < currentFrame->len && i < lastFrame->len; i += 10) {
    int diff = abs(currentFrame->buf[i] - lastFrame->buf[i]);
    if (diff > thresholdDiff) diffCount++;
  }

  esp_camera_fb_return(lastFrame);
  lastFrame = esp_camera_fb_get();

  Serial.printf("Motion diff count: %d\n", diffCount);

  return diffCount > MOTION_THRESHOLD;
}

void streamMJPEG(WiFiClient client) {
  String header = "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(header);

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }

    client.print("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ");
    client.print(fb->len);
    client.print("\r\n\r\n");
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);

    if (!client.connected()) break;

    delay(100);
  }
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  Serial.println("Client connected");

  String request = client.readStringUntil('\r');
  Serial.print("Request: ");
  Serial.println(request);

  if (request.indexOf("GET /stream") >= 0) {
    streamMJPEG(client);
  } else {
    String html = "<html><body>";
    html += "<h1>ESP32-CAM Stream</h1>";
    html += "<img src=\"/stream\" width=\"640\"/>";
    html += "<p>Motion detected: ";
    html += (motionDetected ? "YES" : "NO");
    html += "</p></body></html>";

    client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    client.print(html);
    client.stop();
  }

  Serial.println("Client disconnected");
}

void loop() {
  ArduinoOTA.handle();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  if (millis() - lastMotionCheck > MOTION_CHECK_INTERVAL) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      motionDetected = detectMotion(fb);
      esp_camera_fb_return(fb);
    }
    lastMotionCheck = millis();
  }

  handleClient();
}
