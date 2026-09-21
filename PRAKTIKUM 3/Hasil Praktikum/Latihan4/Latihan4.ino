#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <DHT.h>

const char* ssid = "V2030";
const char* password = "raid_0506";

const char* serverName = "http://10.151.206.38/relay/on";

#define DHTPIN D4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nClient Terhubung ke Wi-Fi!");
}

void loop() {

  float temperature = dht.readTemperature();

  if (isnan(temperature)) {
    Serial.println("Gagal membaca sensor DHT!");
    delay(2000);
    return;
  }

  Serial.print("Suhu: ");
  Serial.print(temperature);
  Serial.println(" °C");

  if ((WiFi.status() == WL_CONNECTED) && (temperature > 29)) {

    WiFiClient client;
    HTTPClient http;

    http.begin(client, serverName);

    int httpResponseCode = http.GET();

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    http.end();

    delay(10000);
  }

  delay(2000);
}