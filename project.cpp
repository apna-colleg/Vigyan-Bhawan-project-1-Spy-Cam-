#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

#define PIR_SENSOR_PIN 13  // Optional: Connect your PIR sensor output to GPIO 13

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server and motion detection settings
WiFiServer server(80);
bool motionDetected = false;
unsigned long lastCheck = 0;

#define MOTION_THRESHOLD 30
#define CHECK_INTERVAL 500

camera_fb_t *lastFrame = nullptr;

#include "camera_pins.h" // AI Thinker camera pinout

void setup() {
  Serial.begin(115200);
  pinMode(PIR_SENSOR_PIN, INPUT);  // Optional PIR motion sensor

  // Camera configuration
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

  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  if(esp_camera_init(&config) != ESP_OK){
    Serial.println("Camera init failed");
    return;
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
  ArduinoOTA.begin();
}

bool detectMotion(camera_fb_t* fb) {
  if (!lastFrame) {
    lastFrame = esp_camera_fb_get();
    return false;
  }

  int diffCount = 0;
  for (size_t i = 0; i < fb->len && i < lastFrame->len; i += 10) {
    if (abs(fb->buf[i] - lastFrame->buf[i]) > 10) {
      diffCount++;
    }
  }

  esp_camera_fb_return(lastFrame);
  lastFrame = esp_camera_fb_get();
  return diffCount > MOTION_THRESHOLD;
}

void streamMJPEG(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) continue;

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.println();

    esp_camera_fb_return(fb);
    delay(100);
  }
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  String req = client.readStringUntil('\r');
  if (req.indexOf("GET /stream") >= 0) {
    streamMJPEG(client);
  } else {
    client.println("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    client.println("<html><body>");
    client.println("<h1>ESP32-CAM Web Stream</h1>");
    client.println("<img src=\"/stream\" width=\"640\">");
    client.print("<p>Motion: ");
    client.print(motionDetected ? "YES" : "NO");
    client.println("</p></body></html>");
  }
  client.stop();
}

void loop() {
  ArduinoOTA.handle();

  // Check PIR sensor
  bool pirTriggered = digitalRead(PIR_SENSOR_PIN) == HIGH;

  // Optional software motion detection
  if (millis() - lastCheck > CHECK_INTERVAL) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
      motionDetected = detectMotion(fb) || pirTriggered;
      esp_camera_fb_return(fb);
    }
    lastCheck = millis();
  }

  handleClient();
}
