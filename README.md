---

Step 1: Hardware Setup
Connect your ESP32-CAM to CP2102 USB-to-Serial converter:

CP2102 Pin	ESP32-CAM Pin
GND	GND
3.3V	3.3V
TX	U0R (GPIO3 / RX)
RX	U0T (GPIO1 / TX)

Do NOT connect 5V directly to ESP32-CAM.

To upload code, hold the BOOT button on ESP32-CAM, press and release RESET, then release BOOT.

Step 2: Replace WiFi Credentials
Find these lines and replace with your WiFi:

cpp
Copy
Edit
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
Example:

cpp
Copy
Edit
const char* ssid = "MyWiFi";
const char* password = "MyPassword123";

Step 3: Configure Arduino IDE
Select Tools > Board > ESP32 Arduino > AI Thinker ESP32-CAM

Select the COM port for your CP2102

Hold BOOT button on ESP32-CAM, click Upload

When "Connecting" appears, press RESET button once and release BOOT

Step 4: Open Serial Monitor
Set baud rate to 115200

Wait for ESP32-CAM to connect to WiFi

It will print the IP address like:

yaml
Copy
Edit
Connected! IP: 192.168.1.45

Step 5: View the Stream
Open a web browser on any device on the same WiFi

Type:

cpp
Copy
Edit
http://<your-esp32-ip>/
Example:

cpp
Copy
Edit
http://192.168.1.45/
You will see the live video stream and motion detection status.

---