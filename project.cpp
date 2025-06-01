#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

#define PIR_PIN 13  // GPIO pin connected to PIR sensor

// Replace with your WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// AI Thinker ESP32-CAM pins
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

WiFiServer server(80);
bool motionDetected = false;

void startCamera() {
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

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
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
    delay(100);
  }
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  Serial.println("Client connected");

  String request = client.readStringUntil('\r');
  Serial.println(request);

  if (request.indexOf("GET /stream") >= 0) {
    streamMJPEG(client);
  } else {
    String html = "<html><body>";
    html += "<h1>ESP32-CAM Stream with PIR Sensor</h1>";
    html += "<img src=\"/stream\" width=\"640\"/>";
    html += "<p>Motion: ";
    html += motionDetected ? "YES" : "NO";
    html += "</p></body></html>";

    client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    client.print(html);
  }

  client.stop();
  Serial.println("Client disconnected");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  startCamera();

  ArduinoOTA.begin();
  server.begin();
  Serial.println("Server started");
}

void loop() {
  ArduinoOTA.handle();

  // Check PIR sensor
  motionDetected = digitalRead(PIR_PIN);
  if (motionDetected) {
    Serial.println("Motion detected!");
  }

  handleClient();
}
