/*
 * ==============================================================================
 * PRAKTIKUM 7: SIMPUL SENSOR-AKTUATOR IOT UNTUK BROKER LOKAL
 * ==============================================================================
 * Program ini membaca sensor DHT11/22, menyusun paket telemetri JSON, mengirimkannya
 * ke Broker Mosquitto Lokal di Laptop, serta mendengarkan instruksi kendali LED.
 * ==============================================================================
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// 1. Konfigurasi Wi-Fi
const char* ssid = "V2030";
const char* wifiPassword = "raid_0508";

// 2. Konfigurasi MQTT Broker Lokal
const char* mqttServer = "192.168.138.195";
const int mqttPort = 1883;

// Kredensial autentikasi Mosquitto
const char* mqttUser = "admin";
const char* mqttPassword = "rahasia123";

// 3. Identitas dan topik MQTT
const char* clientId = "ESP8266_Local_Node01";
const char* topicPubTele = "kampus/labiot/telemetri";
const char* topicSubCmd = "kampus/labiot/kendali";

// 4. Pin perangkat
const byte dhtPin = 2;  // D4 = GPIO 2
const byte ledPin = 5;  // D1 = GPIO 5

#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// Timer
unsigned long lastTeleTime = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;

const unsigned long teleInterval = 4000;
const unsigned long wifiRetryInterval = 10000;
const unsigned long mqttRetryInterval = 4000;

// 5. Callback untuk perintah dari Node-RED
void onMessageCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;

  DeserializationError err =
      deserializeJson(doc, payload, length);

  if (err) {
    Serial.printf("[ERROR] JSON kendali tidak valid: %s\n",
                  err.c_str());
    return;
  }

  if (String(topic) != topicSubCmd) {
    return;
  }

  if (!doc["led"].is<int>()) {
    Serial.println("[ERROR] Perintah LED tidak valid!");
    return;
  }

  int stateLED = doc["led"].as<int>();

  if (stateLED != 0 && stateLED != 1) {
    Serial.println("[ERROR] Nilai LED harus 0 atau 1!");
    return;
  }

  digitalWrite(ledPin, stateLED == 1 ? HIGH : LOW);

  Serial.printf("[KENDALI] LED %s\n",
                stateLED == 1 ? "MENYALA" : "PADAM");
}

// 6. Menghubungkan Wi-Fi tanpa menunggu terus-menerus
void mulaiWiFi() {
  Serial.printf("[Wi-Fi] Menghubungkan ke %s...\n", ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifiPassword);

  lastWiFiAttempt = millis();
}

// 7. Menghubungkan MQTT menggunakan autentikasi
void reconnectMQTT() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (now - lastMQTTAttempt < mqttRetryInterval) {
    return;
  }

  lastMQTTAttempt = now;

  Serial.printf("[MQTT] Menghubungkan ke %s...\n", mqttServer);

  if (client.connect(clientId, mqttUser, mqttPassword)) {
    Serial.println("[MQTT] Terhubung dengan autentikasi sah!");

    if (client.subscribe(topicSubCmd)) {
      Serial.printf("[MQTT] Subscribe: %s\n", topicSubCmd);
    } else {
      Serial.println("[MQTT] Gagal subscribe topik kendali.");
    }

    lastTeleTime = millis();
  } else {
    Serial.printf("[MQTT] Gagal, rc=%d\n", client.state());
    Serial.println("[MQTT] Akan mencoba kembali.");
  }
}

// 8. Setup
void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageCallback);
  client.setBufferSize(512);
  client.setKeepAlive(30);
  client.setSocketTimeout(3);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifiPassword);
  lastWiFiAttempt = millis();

  Serial.println("\n=== PRAKTIKUM 7 MQTT AUTHENTICATION ===");
}

// 9. Loop utama
void loop() {
  unsigned long now = millis();

  // Pulihkan Wi-Fi jika terputus
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWiFiAttempt >= wifiRetryInterval) {
      mulaiWiFi();
    }
    return;
  }

  // Tampilkan alamat IP setelah Wi-Fi tersambung
  static bool wifiInfoPrinted = false;

  if (!wifiInfoPrinted) {
    Serial.println("[Wi-Fi] Berhasil terhubung!");
    Serial.print("[Wi-Fi] IP NodeMCU: ");
    Serial.println(WiFi.localIP());
    wifiInfoPrinted = true;
  }

  // Pulihkan MQTT jika terputus
  if (!client.connected()) {
    reconnectMQTT();
    return;
  }

  // Memproses pesan masuk dan callback
  client.loop();

  // Kirim telemetri setiap 4 detik
  now = millis();

  if (now - lastTeleTime >= teleInterval) {
    lastTeleTime = now;

    float suhu = dht.readTemperature();
    float kelembapan = dht.readHumidity();

    if (isnan(suhu) || isnan(kelembapan)) {
      Serial.println("[ERROR] Gagal membaca sensor DHT22!");
      return;
    }

    StaticJsonDocument<256> doc;

    doc["node_id"] = clientId;
    doc["suhu"] = suhu;
    doc["kelembapan"] = kelembapan;
    doc["uptime_detik"] = millis() / 1000;
    doc["ip_lokal"] = WiFi.localIP().toString();
    doc["status_led"] = digitalRead(ledPin);

    char teleBuffer[256];
    size_t len = serializeJson(doc, teleBuffer, sizeof(teleBuffer));

    if (len > 0 && client.publish(topicPubTele, teleBuffer)) {
      Serial.print("[TELEMETRI TERKIRIM] ");
      Serial.println(teleBuffer);
    } else {
      Serial.println("[ERROR] Gagal mengirim telemetri!");
    }
  }
}
