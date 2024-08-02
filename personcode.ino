#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h> // Include the WiFiManager library
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"

// WiFiManager instance
WiFiManager wifiManager;

// Server configuration
const char* serverName = "192.168.43.192"; // Replace with your server's IP address
String employeePath = "/janimaniabsensi/kirim/uploadkaryawan.php";
String checkEmployeePath = "/janimaniabsensi/kirim/get_karyawan.php";

const int serverPort = 443; // HTTPS port

WiFiClientSecure client;

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_GPIO_NUM     4 // Flash LED pin

const int employeeInterval = 500; // Time between each HTTP POST employee image (2 seconds)

unsigned long previousMillisEmployee = 0; // Last time employee image was sent

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);

  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW); // Ensure flash is off initially

  // Automatically connect using saved credentials, or start configuration portal
  wifiManager.autoConnect("ESP32-CAM");

  Serial.println("Connected to WiFi");
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

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

  // Set to highest resolution supported by the camera
  config.frame_size = FRAMESIZE_UXGA; // 1600x1200
  config.jpeg_quality = 10; // 0-63 lower number means higher quality
  config.fb_count = 2;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Check if it's time to send an employee photo
  if (currentMillis - previousMillisEmployee >= employeeInterval) {
    String filename = checkSendPhoto(checkEmployeePath);
    if (!filename.isEmpty()) {
      sendPhoto(filename, employeePath);
    }
    previousMillisEmployee = currentMillis;
  }
}

String checkSendPhoto(String checkPath) {
  Serial.println("Checking if photo should be sent...");
  client.setInsecure(); // Skip certificate validation
  if (client.connect(serverName, serverPort)) {
    client.println("GET " + checkPath + " HTTP/1.1");
    client.println("Host: " + String(serverName));
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }

    String response = client.readStringUntil('\n');
    client.stop();

    Serial.println("Response: " + response);

    // Check the value of the response string
    if (response.length() > 0) {
      response.trim();  // Remove any leading/trailing whitespace

      Serial.print("Response: ");
      Serial.println(response);
      
      if (response != "0") {  // If the response is not "0", return the filename
        return response + ".jpg";
      }
    }
  } else {
    Serial.println("Connection to check server failed.");
  }
  return "";
}

String sendPhoto(String filename, String path) {
  String getAll;
  String getBody;

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  digitalWrite(FLASH_GPIO_NUM, HIGH); // Turn on flash

  Serial.println("Connecting to server: " + String(serverName));

  client.setInsecure(); // Skip certificate validation
  client.setTimeout(10000); // 10 seconds timeout

  if (client.connect(serverName, serverPort)) {
    Serial.println("Connection successful!");

    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"" + filename + "\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.println("POST " + path + " HTTP/1.1");
    client.println("Host: " + String(serverName));
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    client.print(head);

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n = n + 1024) {
      if (n + 1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      } else if (fbLen % 1024 > 0) {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }
    client.print(tail);

    esp_camera_fb_return(fb);

    Serial.println("Data sent, awaiting response...");

    int timeoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + timeoutTimer) > millis()) {
      Serial.print(".");
      delay(100);
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (getAll.length() == 0) {
            state = true;
          }
          getAll = "";
        } else if (c != '\r') {
          getAll += String(c);
        }
        if (state == true) {
          getBody += String(c);
        }
        startTimer = millis();
      }
      if (getBody.length() > 0) {
        break;
      }
    }
    Serial.println();
    client.stop();
    Serial.println("Response Body: " + getBody); // Detailed response from server
  } else {
    getBody = "Connection to " + String(serverName) + " failed.";
    Serial.println(getBody);
  }

  digitalWrite(FLASH_GPIO_NUM, LOW); // Turn off flash

  return getBody;
}
