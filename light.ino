#include <Wire.h>              // ใช้สำหรับการสื่อสาร I2C
#include <WiFi.h>              // ใช้สำหรับการเชื่อมต่อ Wi-Fi
#include <WiFiClient.h>        // ใช้สำหรับการเชื่อมต่อ Wi-Fi Client
#include <BlynkSimpleEsp32.h>  // ใช้สำหรับการเชื่อมต่อกับ Blynk Server *ให้ใช้ Blynk version 0.60 เท่านั้น
#include <DHT.h>               // ไลบรารีสำหรับเซ็นเซอร์ DHT (ตรวจวัดอุณหภูมิและความชื้น)
#include <HTTPClient.h>        // ใช้สำหรับส่ง HTTP Request
#include <WiFiManager.h>       // 🔹 เพิ่ม WiFiManager

// Blynk credentials
const char auth[] = "lGi7s9Kl1q3b9SAJMK-QKwLgB5I0bOhS"; // Auth token from Blynk app

// 🔹 กำหนด Token และ Chat ID หรือ Chat ID ของกลุ่ม
const char* botToken = "7547625655:AAGBM8wOexys1hwLimBc-Afr3RVCC3RPLvo";  //Telegram HTTP API Token
const char* chatID = "-4773459198";                                       //Chat ID หรือ Chat ID ของกลุ่ม

float tempThreshold = 25.0;      // ค่าอุณหภูมิที่แจ้งเตือน (เริ่มต้น 25°C)
float lightPercentage = 80.0;      // ค่าความเข้มแสงที่แจ้งเตือน (เริ่มต้น 80)

// GPIO configuration
#define LDR_PIN 34  // GPIO34 connected to LDR sensor
#define DHTPIN 15   // GPIO15 connected to DHT sensor
#define DHTTYPE DHT22 // DHT type: DHT11 or DHT22

// Create DHT instance
DHT dht(DHTPIN, DHTTYPE);

// Timer for periodic tasks
BlynkTimer timer;

// Function prototypes
void readSensors();

// ตัวแปรสำหรับควบคุมการส่งข้อความ Telegram
unsigned long lastTelegramSent = 0;  // เวลาในการส่งข้อความล่าสุด
const unsigned long TELEGRAM_COOLDOWN = 10000;  // หน่วงเวลา 10 วินาที (10000 มิลลิวินาที)

void sendTelegramMessage(String alertType, float value) {
    if (WiFi.status() != WL_CONNECTED) return;
    // เช็กว่าผ่านเวลาหน่วงแล้วหรือยัง
    if (millis() - lastTelegramSent < TELEGRAM_COOLDOWN) return;  

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String message = "{\"chat_id\":\"" + String(chatID) + "\",\"text\":\"🚨 แจ้งเตือน: " 
                     + alertType + "\\nค่าปัจจุบัน: " + String(value) + "\"}";

    int httpResponseCode = http.POST(message);
    
    if (httpResponseCode > 0) {
        Serial.println("✅ ส่งข้อความไปยัง Telegram สำเร็จ");
    } else {
        Serial.print("❌ ไม่สามารถส่งข้อความ: ");
        Serial.println(httpResponseCode);
    }

    http.end();
    lastTelegramSent = millis();  // อัปเดตเวลาเพื่อป้องกันการส่งซ้ำ
}

// 🔹 ฟังก์ชันสำหรับตั้งค่า Wi-Fi อัตโนมัติ
void setupWiFi() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("Sompoch_smartFarm");  // เปิด Hotspot ให้ตั้งค่า Wi-Fi เองหากไม่มีการเชื่อมต่อ Wi-Fi ที่เคยเชื่อมต่อได้
  Serial.println("✅ Wi-Fi Connected!");
}

void setup() {
  Serial.begin(9600);
  pinMode(LDR_PIN, INPUT);
  dht.begin();

  // เชื่อมต่อ Wi-Fi โดยใช้ WiFiManager
  setupWiFi();

  Serial.println("Connecting to Blynk server...");
  Blynk.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str(), "iotservices.thddns.net", 5535);  // ตัวอย่าง Blynk Public Server

  timer.setInterval(2000L, readSensors);   // Read sensors every 15 minute
}

BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");
  Blynk.syncAll();
  Serial.println("LED state set to: ON (forced ON after connection)");
}

void loop() {
    if (Blynk.connected()) {
        Blynk.run();
    }

    // เรียกใช้ฟังก์ชัน readSensors เพื่ออ่านข้อมูลจากเซ็นเซอร์และส่งข้อความ
    readSensors();

    delay(1000);
}

void readSensors() {
    // อ่านค่าความเข้มแสง (กลับค่าให้มืด = 0%, สว่าง = 100%)
    int lightValue = analogRead(LDR_PIN);
    float lightPercentage = (1 - (lightValue / 4095.0)) * 100.0;
    Blynk.virtualWrite(V3, lightPercentage);

    // อ่านค่าอุณหภูมิและความชื้น
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    // ตรวจสอบว่าค่าไม่เป็น NaN
    if (!isnan(temperature) && !isnan(humidity)) {
        Blynk.virtualWrite(V1, temperature);
        Blynk.virtualWrite(V2, humidity);
        Serial.printf("🌡 อุณหภูมิ: %.2f °C, 💧 ความชื้น: %.2f %%, 💡 ความเข้มแสง: %.2f%%\n", 
                      temperature, humidity, lightPercentage);
    }

    // 🔹 ตรวจสอบค่าแล้วแจ้งเตือนผ่าน Telegram
    if (lightPercentage > 80.0) {  // เมื่อความเข้มแสงมากกว่า 80% (สว่างจ้า)
        sendTelegramMessage("💡 ความเข้มแสงสูงเกินกำหนด!", lightPercentage);
    }
    if (temperature > 25.0) {
        sendTelegramMessage("🌡 อุณหภูมิสูงเกิน 25°C", temperature);
    }
    if (humidity < 40.0) {
        sendTelegramMessage("💧 ความชื้นต่ำเกิน 40%", humidity);
    }
}
