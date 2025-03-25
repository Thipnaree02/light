#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiManager.h>

// Blynk credentials
const char auth[] = "lGi7s9Kl1q3b9SAJMK-QKwLgB5I0bOhS";

// Telegram credentials
const char* botToken = "7547625655:AAGBM8wOexys1hwLimBc-Afr3RVCC3RPLvo";
const char* chatID = "-4773459198";

float tempThreshold = 25.0;
float lightThreshold = 80.0;
float humidityThreshold = 40.0;

// GPIO configuration
#define LDR_PIN 34
#define DHTPIN 15
#define DHTTYPE DHT22

// Create DHT instance
DHT dht(DHTPIN, DHTTYPE);

// Timer for periodic tasks
BlynkTimer timer;

// ตัวแปรสำหรับควบคุมการส่งข้อความ Telegram
unsigned long lastTelegramSent = 0;
const unsigned long TELEGRAM_COOLDOWN = 10000;

void sendTelegramMessage(String alertType, float value) {
    if (WiFi.status() != WL_CONNECTED) return;
    if (millis() - lastTelegramSent < TELEGRAM_COOLDOWN) return;

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String message = "{\"chat_id\":\"" + String(chatID) + "\",\"text\":\"🚨 แจ้งเตือน: " 
                     + alertType + "\nค่าปัจจุบัน: " + String(value) + "\"}";

    int httpResponseCode = http.POST(message);
    
    if (httpResponseCode > 0) {
        Serial.println("✅ ส่งข้อความไปยัง Telegram สำเร็จ");
    } else {
        Serial.print("❌ ไม่สามารถส่งข้อความ: ");
        Serial.println(httpResponseCode);
    }

    http.end();
    lastTelegramSent = millis();
}

void setupWiFi() {
    WiFiManager wifiManager;
    wifiManager.autoConnect("thipnaree_hotspot");
    Serial.println("✅ Wi-Fi Connected!");
}

void setup() {
    Serial.begin(9600);
    pinMode(LDR_PIN, INPUT);
    dht.begin();

    setupWiFi();
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    Serial.println("Connecting to Blynk server...");
    Blynk.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str(), "iotservices.thddns.net", 5535);

    timer.setInterval(2000L, readSensors);
}

BLYNK_CONNECTED() {
    Serial.println("Blynk connected!");
    Blynk.syncAll();
}

BLYNK_WRITE(V4) {
    tempThreshold = param.asFloat();
    Serial.print("🛠 ปรับค่าแจ้งเตือนอุณหภูมิเป็น: ");
    Serial.println(tempThreshold);
}

BLYNK_WRITE(V5) {
    humidityThreshold = param.asFloat();
    Serial.print("🛠 ปรับค่าแจ้งเตือนความชื้นเป็น: ");
    Serial.println(humidityThreshold);
}

BLYNK_WRITE(V6) {
    lightThreshold = param.asFloat();
    Serial.print("🛠 ปรับค่าแจ้งเตือนความเข้มแสงเป็น: ");
    Serial.println(lightThreshold);
}

void loop() {
    if (Blynk.connected()) {
        Blynk.run();
    }
    timer.run();
}

void readSensors() {
    int lightValue = analogRead(LDR_PIN);
    float lightPercentage = (1 - (lightValue / 4095.0)) * 100.0;
    Blynk.virtualWrite(V3, lightPercentage);

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity)) {
        Blynk.virtualWrite(V1, temperature);
        Blynk.virtualWrite(V2, humidity);
        Blynk.virtualWrite(V3, lightPercentage);
        Serial.printf("🌡 อุณหภูมิ: %.2f °C, 💧 ความชื้น: %.2f %%, 💡 ความเข้มแสง: %.2f%%\n", 
                      temperature, humidity, lightPercentage);
    }

    if (lightPercentage > lightThreshold) {
        sendTelegramMessage("💡 ความเข้มแสงสูงเกินกำหนด!", lightPercentage);
    }
    if (temperature > tempThreshold) {
        sendTelegramMessage("🌡 อุณหภูมิสูงเกินกำหนด", temperature);
    }
    if (humidity < humidityThreshold) {
        sendTelegramMessage("💧 ความชื้นต่ำเกินกำหนด", humidity);
    }
}

void reconnectWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("🔄 Reconnecting Wi-Fi...");
        WiFiManager wifiManager;
        wifiManager.autoConnect("ESP32_Config");
        Serial.println("✅ Wi-Fi Reconnected!");
    }
}
