#include <DHT.h>

// Pemetaan Pin untuk ESP32
const byte ldrPin   = 36;  // Pin VP (GPIO 36) untuk Analog LDR
const byte dhtPin   = 13;  // Pin 13 (GPIO 13) untuk Data DHT
const byte relayPin = 12;  // Pin 12 (GPIO 12) untuk Signal Relay
const byte ledPin   = 19;  // Pin 19 (GPIO 19) untuk LED Indikator

#define DHTTYPE DHT22        
DHT dht(dhtPin, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  // Konfigurasi pin aktuator sebagai OUTPUT
  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  
  // Keamanan: Pastikan relay dan LED mati di awal pembacaan
  digitalWrite(relayPin, LOW); 
  digitalWrite(ledPin, LOW);
  
  Serial.println("--- System Warehouse Edge Controller (ESP32) Dimulai ---");
}

void loop() {
  // Jedapembacaan sensor DHT (2 detik)
  delay(2000); 

  // 1. Membaca data dari multi-sensor
  int ldrValue = analogRead(ldrPin);
  float temp   = dht.readTemperature();
  float hum    = dht.readHumidity();

  // 2. Proteksi Error Handling jika pembacaan DHT gagal
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Error: Gagal membaca data dari sensor DHT!");
    return;
  }

  // 3. Menampilkan data telemetri ke Serial Monitor
  Serial.print("Suhu: ");
  Serial.print(temp);
  Serial.print(" °C | Kelembapan: ");
  Serial.print(hum);
  Serial.print(" % | Cahaya (ADC): ");
  Serial.println(ldrValue);

  // 4. Rule Engine (Kondisi OR: Suhu > 34°C ATAU LDR < 300)
  if (temp > 34.0 || ldrValue < 300) {
    digitalWrite(relayPin, HIGH); // Aktifkan Relay
    digitalWrite(ledPin, HIGH);   // Nyalakan LED Indikator
    Serial.println(" Status: PERINGATAN! Kondisi Buruk Terdeteksi. Aktuator Aktif!");
  } else {
    digitalWrite(relayPin, LOW);  // Matikan Relay
    digitalWrite(ledPin, LOW);    // Matikan LED Indikator
    Serial.println(" Status: Kondisi Aman.");
  }
  
  Serial.println("----------------------------------------------------");
}