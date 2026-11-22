// soil_dht22_cloud.ino
// ESP32 + DHT22 (GPIO 4) + soil moisture sensor (GPIO 34)
// Prints readings every 2 s and sends them to ThingSpeak over HTTPS every 20 s.
//
// ThingSpeak fields (agree these with Person 6):
//   field1 = temperature, field2 = humidity, field3 = soil_moisture
//
// DO NOT commit your real WiFi password or API key to GitHub.

#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define DHTPIN   4
#define DHTTYPE  DHT22
#define SOIL_PIN 34
#define ADC_MAX  4095

// ---- FILL THESE IN ----
const char* WIFI_SSID = "YOUR_WIFI_NAME";        // 2.4 GHz network only
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* API_KEY   = "PERSON6_WRITE_API_KEY"; // ThingSpeak > Channel > API Keys
// -----------------------

// DRY_VALUE = raw reading in dry air, WET_VALUE = raw reading in water.
const int DRY_VALUE = 2608;
const int WET_VALUE = 10;

const unsigned long SEND_INTERVAL = 20000;  // ThingSpeak free plan: min 15 s
unsigned long lastSend = 0;

DHT dht(DHTPIN, DHTTYPE);

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" connected");
  } else {
    Serial.println(" FAILED (will retry; sensors still work)");
  }
}

void sendToThingSpeak(float temperature, float humidity, int soilPct) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi, reconnecting...");
    connectWiFi();
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();   // encrypts traffic; does not verify the server's certificate

  HTTPClient https;
  https.begin(client, "https://api.thingspeak.com/update");
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // The key goes in the POST body, not in the URL
  String body = "api_key=" + String(API_KEY)
              + "&field1=" + String(temperature, 1)
              + "&field2=" + String(humidity, 1)
              + "&field3=" + String(soilPct);

  int code = https.POST(body);
  String reply = https.getString();   // entry number on success, "0" if rejected

  Serial.print("ThingSpeak response: ");
  Serial.print(code);
  Serial.print(" | entry: ");
  Serial.println(reply);              // 200 and entry > 0 = success
  https.end();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Serial.println("Soil + DHT22 + cloud starting...");
  connectWiFi();
}

void loop() {
  delay(2000);

  float humidity    = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Raw soil reading (0 to 4095) and the same reading as a % of full scale
  int   soilRaw    = analogRead(SOIL_PIN);
  float rawPercent = (soilRaw * 100.0) / ADC_MAX;

  // Calibrated soil moisture: 0 = dry, 100 = wet
  int soilPct = map(soilRaw, DRY_VALUE, WET_VALUE, 0, 100);
  soilPct = constrain(soilPct, 0, 100);

  bool dhtOk = !(isnan(humidity) || isnan(temperature));

  if (!dhtOk) {
    Serial.println("Failed to read from DHT22");
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" C | Humidity: ");
    Serial.print(humidity);
    Serial.print(" % | ");
  }

  Serial.print("Soil raw: ");
  Serial.print(soilRaw);
  Serial.print(" (");
  Serial.print(rawPercent, 1);
  Serial.print(" % of range) | Soil moisture: ");
  Serial.print(soilPct);
  Serial.println(" %");

  if (dhtOk && millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    sendToThingSpeak(temperature, humidity, soilPct);
  }
}
