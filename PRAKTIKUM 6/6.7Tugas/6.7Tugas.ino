
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid = "Aku suka kamu";
const char* password = "Kamusukasamayanglain";

const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

const char* clientId = "ESP8266_LabIoT_25782069_Safety";

const char* topicTelemetry =
    "kampus/labiot/25782069/telemetri";
const char* topicControl =
    "kampus/labiot/25782069/kendali";
const char* topicAlert =
    "kampus/labiot/25782069/alert";
const char* topicStatus =
    "kampus/labiot/25782069/status";

// Pesan Last Will and Testament
const char* willPayload =
    "{\"status\":\"OFFLINE\","
    "\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";

// Pesan status saat perangkat berhasil terhubung
const char* onlinePayload =
    "{\"status\":\"ONLINE\","
    "\"keterangan\":\"Perangkat Beroperasi Normal\"}";

// ==================== KONFIGURASI SENSOR ====================
#define DHTPIN 2
#define DHTTYPE DHT22 

const int ledPin = 5;   // D1 / GPIO5
const float batasSuhu = 31.0;

DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

// ==================== VARIABEL SISTEM ====================
float suhu = NAN;
float kelembapan = NAN;

bool kondisiBahaya = false;
bool alertLatched = false;
bool ledManual = false;

unsigned long lastDhtRead = 0;
unsigned long lastTelemetry = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;

const unsigned long intervalDHT = 2000;
const unsigned long intervalTelemetry = 4000;
const unsigned long intervalWiFi = 10000;
const unsigned long intervalMQTT = 3000;

// ==================== KENDALI LED ====================
void updateLED() {
  // Keselamatan otomatis memiliki prioritas tertinggi
  if (kondisiBahaya) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, ledManual ? HIGH : LOW);
  }
}

// ==================== CALLBACK MQTT ====================
void callback(char* topic, byte* payload,
              unsigned int length) {
  if (String(topic) != topicControl) {
    return;
  }

  // Buffer untuk payload JSON yang diterima
  char message[128];

  if (length >= sizeof(message)) {
    Serial.println("Pesan kendali terlalu panjang.");
    return;
  }

  memcpy(message, payload, length);
  message[length] = '\0';

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.println("JSON kendali tidak valid.");
    return;
  }

  if (!doc["led"].is<int>()) {
    Serial.println("Perintah LED tidak ditemukan.");
    return;
  }

  int perintahLED = doc["led"];

  if (perintahLED != 0 && perintahLED != 1) {
    Serial.println("Nilai LED harus 0 atau 1.");
    return;
  }

  if (kondisiBahaya) {
    Serial.println(
        "KENDALI MANUAL DITOLAK: SUHU BAHAYA!");
    updateLED();
    return;
  }

  ledManual = (perintahLED == 1);
  updateLED();

  Serial.print("Kendali manual LED: ");
  Serial.println(ledManual ? "MENYALA" : "MATI");
}

// ==================== KONEKSI WIFI ====================
void mulaiWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  lastWiFiAttempt = millis();
  Serial.println("Memulai koneksi WiFi...");
}

// ==================== KONEKSI MQTT ====================
void reconnectMQTT() {
  unsigned long now = millis();

  if (now - lastMQTTAttempt < intervalMQTT) {
    return;
  }

  lastMQTTAttempt = now;

  Serial.print("Mencoba koneksi MQTT...");

  // Mendaftarkan LWT: QoS 1 dan retained
  bool connected = client.connect(
      clientId,
      topicStatus,
      1,
      true,
      willPayload
  );

  if (connected) {
    Serial.println("BERHASIL!");

    // Langganan topik kendali
    if (client.subscribe(topicControl)) {
      Serial.println("Subscribe kendali berhasil.");
    } else {
      Serial.println("Subscribe kendali gagal.");
    }

    // Publikasikan status ONLINE setelah tersambung
    if (client.publish(topicStatus, onlinePayload, true)) {
      Serial.println("Status ONLINE terkirim.");
    }

    // Paksa pembacaan dan publikasi telemetri berikutnya
    lastTelemetry = 0;
  } else {
    Serial.print("GAGAL, kode: ");
    Serial.println(client.state());
  }
}

// ==================== BACA SENSOR ====================
void bacaSensor() {
  unsigned long now = millis();

  if (now - lastDhtRead < intervalDHT) {
    return;
  }

  lastDhtRead = now;

  float pembacaanSuhu = dht.readTemperature();
  float pembacaanKelembapan = dht.readHumidity();

  if (isnan(pembacaanSuhu) ||
      isnan(pembacaanKelembapan)) {
    Serial.println("Pembacaan sensor DHT gagal.");
    return;
  }

  suhu = pembacaanSuhu;
  kelembapan = pembacaanKelembapan;

  bool bahayaSebelumnya = kondisiBahaya;
  kondisiBahaya = (suhu > batasSuhu);

  // Masuk kondisi bahaya: LED otomatis menyala
  if (kondisiBahaya && !bahayaSebelumnya) {
    Serial.println("PERINGATAN: SUHU PANAS!");
  }

  // Saat kembali normal, izinkan peringatan baru
  // jika suhu kembali melewati ambang batas
  if (!kondisiBahaya) {
    alertLatched = false;
  }

  updateLED();

  Serial.print("Suhu: ");
  Serial.print(suhu);
  Serial.print(" C | Kelembapan: ");
  Serial.print(kelembapan);
  Serial.print(" % | Kondisi: ");
  Serial.println(kondisiBahaya ? "PANAS" : "NORMAL");
}

// ==================== PUBLIKASI ALERT ====================
void publishAlert() {
  if (!client.connected()) {
    Serial.println("ALERT: MQTT belum terhubung.");
    return;
  }

  if (isnan(suhu)) {
    Serial.println("ALERT: Data suhu belum valid.");
    return;
  }

  if (suhu <= batasSuhu) {
    alertLatched = false;
    return;
  }

  if (alertLatched) {
    return;
  }

  StaticJsonDocument<192> doc;
  doc["status"] = "BAHAYA";
  doc["parameter"] = "OVERHEAT";
  doc["nilai_suhu"] = suhu;

  char buffer[192];
  serializeJson(doc, buffer, sizeof(buffer));

  Serial.print("Mencoba mengirim ALERT: ");
  Serial.println(buffer);

  if (client.publish(topicAlert, buffer)) {
    alertLatched = true;
    Serial.println("ALERT BERHASIL DIKIRIM!");
  } else {
    Serial.println("ALERT GAGAL DIKIRIM!");
  }
}


// ==================== PUBLIKASI TELEMETRI ====================
void publishTelemetry() {
  unsigned long now = millis();

  if (now - lastTelemetry < intervalTelemetry) {
    return;
  }

  if (!client.connected() || isnan(suhu) ||
      isnan(kelembapan)) {
    return;
  }

  lastTelemetry = now;

  StaticJsonDocument<256> doc;

  doc["node_id"] = "ESP8266_25782069";
  doc["suhu"] = suhu;
  doc["kelembapan"] = kelembapan;
  doc["uptime_detik"] = millis() / 1000;
  doc["ip_lokal"] = WiFi.localIP().toString();
  doc["kondisi_suhu"] =
      kondisiBahaya ? "PANAS" : "NORMAL";

  char buffer[256];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));

  if (n > 0 && client.publish(topicTelemetry, buffer)) {
    Serial.print("TELEMETRI: ");
    Serial.println(buffer);
  } else {
    Serial.println("Publikasi telemetri gagal.");
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.setKeepAlive(15);
  client.setSocketTimeout(3);

  mulaiWiFi();
}

// ==================== LOOP NON-BLOCKING ====================
void loop() {
  unsigned long now = millis();

  // Pulihkan koneksi WiFi tanpa delay()
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWiFiAttempt >= intervalWiFi) {
      mulaiWiFi();
    }
    return;
  }

  // Pulihkan koneksi MQTT jika terputus
  if (!client.connected()) {
    reconnectMQTT();
    return;
  }

  // Memproses pesan MQTT dan callback
  client.loop();

  // Jalankan pembacaan sensor secara berkala
  bacaSensor();

  // Peringatan otomatis ketika suhu melewati batas
  publishAlert();

  // Telemetri rutin
  publishTelemetry();
}
