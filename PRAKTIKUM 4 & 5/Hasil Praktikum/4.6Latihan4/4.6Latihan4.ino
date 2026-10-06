#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

const char* ssid = "V2030";
const char* password = "raid_0507";

// Konfigurasi Pin
const byte dhtPin = 2;        // D4 (GPIO 2)
const byte buttonPin = 4;     // D2 (GPIO 4)
const byte ledPin = 12;       // D6 (GPIO 12)

DHT dht(dhtPin, DHT22);

// Variabel Pelacak Status (State & Cache)
bool ledState = false;
String currentTemp = "--";
String currentHum  = "--";

int buttonState;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long lastTime = 0;

// Inisialisasi Async Web Server (port 80) & WebSocket (rute /ws)
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------------- HTML & JAVASCRIPT (FRONT-END) TEMA NAVY & KUNING ----------------
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Room - Navy & Yellow</title>
  <style>
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }
    body { 
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
      text-align: center; 
      background-color: #0f172a; /* Navy Blue gelap */
      color: #f8fafc;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    h1 {
      color: #f59e0b; /* Aksen Kuning / Gold */
      margin-bottom: 25px;
      font-size: 2rem;
      letter-spacing: 1px;
      text-shadow: 0 2px 4px rgba(0,0,0,0.4);
    }
    .card { 
      background: #1e293b; /* Navy Blue Sedang */
      margin: 12px auto; 
      padding: 22px; 
      width: 100%;
      max-width: 320px; 
      border-radius: 16px; 
      box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.5), 0 8px 10px -6px rgba(0, 0, 0, 0.3);
      border: 1px solid #334155;
      transition: transform 0.2s ease, box-shadow 0.2s ease;
    }
    .card:hover {
      transform: translateY(-3px);
      box-shadow: 0 14px 28px -5px rgba(0, 0, 0, 0.6);
    }
    .card h2 {
      font-size: 1.1rem;
      font-weight: 500;
      color: #94a3b8;
      margin-bottom: 8px;
    }
    .value {
      font-size: 2rem;
      font-weight: 700;
      color: #fbbf24; /* Teks Angka Kuning */
    }
    .status-text {
      font-size: 1.5rem;
      font-weight: 700;
      margin-bottom: 15px;
    }
    .status-off {
      color: #94a3b8;
    }
    .status-on {
      color: #f59e0b; /* Kuning Terang saat LED ON */
    }
    button { 
      padding: 14px 28px; 
      font-size: 1rem; 
      font-weight: 600;
      border-radius: 10px; 
      cursor: pointer; 
      border: none;
      width: 100%;
      transition: all 0.2s ease;
      letter-spacing: 0.5px;
    }
    .btn-off { 
      background-color: #f59e0b; /* Tombol Kuning untuk Turn ON */
      color: #0f172a; 
      box-shadow: 0 4px 14px rgba(245, 158, 11, 0.4);
    }
    .btn-off:hover {
      background-color: #d97706;
    }
    .btn-on { 
      background-color: #334155; /* Tombol Navy Redup untuk Turn OFF */
      color: #fbbf24;
      border: 1px solid #f59e0b;
      box-shadow: 0 4px 14px rgba(0, 0, 0, 0.3);
    }
    .btn-on:hover {
      background-color: #1e293b;
    }
  </style>
</head>
<body>
  <h1>Smart Room</h1>
  
  <div class="card">
    <h2>SUHU</h2>
    <div class="value"><span id="tempValue">--</span> &deg;C</div>
  </div>
  
  <div class="card">
    <h2>KELEMBAPAN</h2>
    <div class="value"><span id="humValue">--</span> %</div>
  </div>
  
  <div class="card">
    <h2>STATUS LED</h2>
    <div id="ledStatus" class="status-text status-off">OFF</div>
    <button id="toggleBtn" class="btn-off" onclick="toggleLed()">Turn ON</button>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    window.addEventListener('load', onLoad);

    function onLoad(event) { initWebSocket(); }

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen    = onOpen;
      websocket.onclose   = onClose;
      websocket.onmessage = onMessage;
    }

    function onOpen(event) { console.log('WebSocket Terkoneksi'); }
    function onClose(event) { setTimeout(initWebSocket, 2000); }

    function toggleLed(){
        websocket.send('toggle');
    }

    function onMessage(event) {
      var dataObj = JSON.parse(event.data);
            
      if(dataObj.suhu !== undefined) { 
        document.getElementById('tempValue').innerHTML = dataObj.suhu;
      }
      
      if(dataObj.hum !== undefined) { 
        document.getElementById('humValue').innerHTML = dataObj.hum;
      }
            
      if(dataObj.led !== undefined) { 
        var btn = document.getElementById('toggleBtn');
        var status = document.getElementById('ledStatus');
        if(dataObj.led == "1"){
          status.innerHTML = "ON";
          status.className = "status-text status-on";
          btn.innerHTML = "Turn OFF";
          btn.className = "btn-on";
        } else {
          status.innerHTML = "OFF";
          status.className = "status-text status-off";
          btn.innerHTML = "Turn ON";
          btn.className = "btn-off";
        }
      }
    }
  </script>
</body>
</html>
)rawliteral";

// ---------------- BACK-END & WEBSOCKET LOGIC ----------------
void notifyClients() {
  String jsonString = "{\"led\":\"" + String(ledState ? 1 : 0) + "\", ";
  jsonString += "\"suhu\":\"" + currentTemp + "\", ";
  jsonString += "\"hum\":\"" + currentHum + "\"}";
  ws.textAll(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) {
      ledState = !ledState;
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Client WebSocket #%u terhubung\n", client->id());
      notifyClients();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Client WebSocket #%u terputus\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP Address: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  ws.cleanupClients();

  digitalWrite(ledPin, ledState ? HIGH : LOW);

  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == HIGH) {
        ledState = !ledState;
        notifyClients();
      }
    }
  }
  lastButtonState = reading;

  if ((millis() - lastTime) > 3000) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if(!isnan(t) && !isnan(h)) {
      currentTemp = String(t);
      currentHum  = String(h);
      notifyClients();
    }
    lastTime = millis();
  }
}