# IAQ-IoT-ESP32

This software is well commented with explanations and instructions to allow for simple reproducibility.

To connect the ESP32 to your Wi-Fi network ensure to change the following lines of code to suit your network:

**char ssid[] = "YOUR_NETWORK_SSID";*

**const char* password = "YOUR_NETWORK_PASSWORD";*

Furthermore, the following line will also need altered to send the sensor data via a POST request to your server:

**const char* serverName = "http://192.168.1.66/IAQ-IoT-Project-WebApp-main/BME680KitchenData.php";*

*The ip address in the line above will need changed to suit your own server.*
