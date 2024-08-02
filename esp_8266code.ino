#ifdef ESP32
  #include <WiFi.h>
  #include <HTTPClient.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #include <WiFiClient.h>
#endif
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DNSServer.h>
#include <WiFiManager.h>

#define SS_PIN 15
#define RST_PIN 16
#define BUZZER_PIN 0  // Pin untuk buzzer

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance.
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD address to 0x27 for a 16 chars and 2 line display

unsigned long buzzerStartTime = 0;  // Variabel untuk menyimpan waktu mulai buzzer
bool buzzerActive = false;          // Status buzzer aktif atau tidak

String kode;
String serverName = "http://192.168.43.192/janimaniabsensi/kirim/setdata.php"; 
String serverName1 = "http://192.168.43.192/janimaniabsensi/kirim/getdata.php"; 

void setup() {
  Serial.begin(9600); // Initialize serial communications with the PC
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  Serial.println("Scan an RFID card");

  pinMode(BUZZER_PIN, OUTPUT); // Set pin mode for buzzer
  digitalWrite(BUZZER_PIN, LOW); // Make sure buzzer is off

  initializeLCD();    // Initialize the LCD

  // Initialize WiFi using WiFiManager

  WiFiManager wifiManager;
  // wifiManager.resetSettings();
  wifiManager.autoConnect("absensi"); // Create an access point for configuration
  Serial.println("Connected to WiFi");
  updateWiFiStatus(); // Update WiFi status on LCD
  delay(2000); // Delay for 2 seconds to display WiFi status
  displayScanMessage(); // Display "Tap Kartu Anda" after WiFi is connected
}

void loop() {
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {
    handleBuzzer();
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    handleBuzzer();
    return;
  }

  // Show UID on serial monitor and LCD
  Serial.print("UID tag: ");
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  displayUID(content);  // Display the UID on the LCD

  // Store the UID in the 'kode' variable and send the HTTP request
  kode = content;
  kirim();

  // Turn on the buzzer for 1 second using millis()
  buzzerStartTime = millis();
  buzzerActive = true;
  digitalWrite(BUZZER_PIN, HIGH);
  delay(2000); // Tambahkan penundaan 2 detik untuk menampilkan UID pada LCD
  get_nama();
  delay(2000); // Tambahkan penundaan 2 detik untuk menampilkan UID pada LCD
  displayScanMessage(); // Kembali ke pesan "Scan kartu Anda"
}

void handleBuzzer() {
  if (buzzerActive && (millis() - buzzerStartTime >= 1000)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}

void initializeLCD() {
  lcd.init(); // Initialize the lcd
  lcd.backlight(); // Turn on the backlight
  lcd.clear(); // Clear the display
}

void displayScanMessage() {
  lcd.clear(); // Clear the display
  lcd.setCursor(0, 0); // Set cursor to the first column, first row
  lcd.print("Tap Kartu Anda");
}

void displayUID(String uid) {
  lcd.clear(); // Clear the display
  lcd.setCursor(0, 0); // Set cursor to the first column, first row
  lcd.print("UID:");
  lcd.setCursor(0, 1); // Set cursor to the first column, second row
  lcd.print(uid);
}

void kirim(){
  if(WiFi.status() == WL_CONNECTED){          
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverName);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String httpRequestData = "kode=" + kode;    
    int httpResponseCode = http.POST(httpRequestData);
    if (httpResponseCode == 200) {
      Serial.println("Data diterima");
    } else {
      Serial.println("Gagal menerima data");
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void get_nama() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http; // Create an HTTPClient object
    WiFiClient client; // Create a WiFiClient object
    http.begin(client, serverName1); // Specify the URL
    int httpResponseCode = http.GET(); // Send the GET request
    
    if (httpResponseCode == 200) { // Check if the response code is 200 (OK)
      String payload = http.getString(); // Get the response payload
      Serial.println("Payload: " + payload); // Print payload to the Serial Monitor
      
      // Check if the payload matches a certain pattern or value
      if (payload.length() > 0) { // Example condition: payload is not empty
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nama Karyawan");
        lcd.setCursor(0, 1);
        lcd.print(payload);
      } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Nama Karyawan");
      lcd.setCursor(0, 1);
      lcd.print("Tidak Ada");
      }
    } else {
      Serial.println("Failed to retrieve data");
      displayScanMessage();
    }
    
    http.end(); // End the HTTP request
  } else {
    Serial.println("WiFi Disconnected");
    displayScanMessage();
  }
}


void updateWiFiStatus() {
  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(0, 0);
    lcd.print("WiFi: Connected");
    lcd.setCursor(0, 1);
    lcd.print("SSID: " + WiFi.SSID());
  } else {
    lcd.setCursor(0, 0);
    lcd.print("WiFi: Disconnected");
  }
}
