
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Konfigurasi WiFi
const char* ssid = "Aku suka kamu";
const char* password = "Kamusukasamayanglain";

// Konfigurasi MQTT
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

const char* clientId = "ESP8266_LabIoT_25782069_Lat3";
const char* topicPublish = "kampus/labiot/25782069/telemetri";

// Konfigurasi sensor DHT
#define DHTPIN 2
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

// Menghubungkan ESP8266 ke WiFi
void setupWiFi() {
  Serial.print("Menghubungkan ke WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi terhubung!");
  Serial.print("IP lokal: ");
  Serial.println(WiFi.localIP());
}

// Menghubungkan ESP8266 ke broker MQTT
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");

    if (client.connect(clientId)) {
      Serial.println("berhasil!");
    } else {
      Serial.print("gagal, kode: ");
      Serial.print(client.state());
      Serial.println(". Coba lagi dalam 3 detik.");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  dht.begin();
  setupWiFi();

  client.setServer(mqttServer, mqttPort);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();

  static unsigned long lastPublish = 0;
  unsigned long now = millis();

  // Mengirim telemetri setiap 4 detik
  if (now - lastPublish >= 4000) {
    lastPublish = now;

    float suhu = dht.readTemperature();
    float kelembapan = dht.readHumidity();

    if (isnan(suhu) || isnan(kelembapan)) {
      Serial.println("Gagal membaca sensor DHT!");
      return;
    }

    // Membentuk data JSON
    JsonDocument jsonDoc;

    jsonDoc["node_id"] = "ESP8266_25782069";
    jsonDoc["suhu"] = suhu;
    jsonDoc["kelembapan"] = kelembapan;

    // Tambahan Latihan 3
    jsonDoc["uptime_detik"] = millis() / 1000;
    jsonDoc["ip_lokal"] = WiFi.localIP().toString();

    // Serialisasi JSON
    char buffer[256];
    serializeJson(jsonDoc, buffer);

    // Mengirim data ke broker MQTT
    bool berhasil = client.publish(topicPublish, buffer);

    Serial.println("Data telemetri:");
    Serial.println(buffer);

    if (berhasil) {
      Serial.println("Publish MQTT berhasil.");
    } else {
      Serial.println("Publish MQTT gagal.");
    }
  }
}
