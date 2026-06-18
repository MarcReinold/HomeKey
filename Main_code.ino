#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// ==========================================
// 1. NETZWERK KONFIGURATION
// ==========================================
const char* ssid     = "IOT";
const char* password = "20tgmiot18";

const String urlFloridsdorf  = "https://www.wienerlinien.at/ogd_realtime/monitor?stopId=4641";
const String urlSiebenhirten = "https://www.wienerlinien.at/ogd_realtime/monitor?stopId=4650";

// ==========================================
// 2. MQTT KONFIGURATION (Broker & Themen)
// ==========================================
const char* mqtt_server = "10.200.0.207";
const int   mqtt_port   = 1883;

const int numKeys = 3;
const char* keyNames[numKeys] = {"Kilian", "Marc", "Patrik"};
String stateTopics[numKeys]; // Wird im Setup dynamisch befüllt

// Display Switch MQTT Topics
const char* mqtt_switch_config_topic = "homeassistant/switch/homekey_display/config";
const char* mqtt_switch_state_topic  = "schluesselbrett/display/state";
const char* mqtt_switch_cmd_topic    = "schluesselbrett/display/set";

WiFiClient   espMqttClient;
PubSubClient mqttClient(espMqttClient);

// ==========================================
// 3. HARDWARE PIN DEFINITIONEN (SENSOREN)
// ==========================================
// GPIO 13 = Kilian, GPIO 12 = Marc, GPIO 11 = Patrik
const int reedPins[numKeys] = {10, 12, 11};

// Patrik hat das KY-021 Modul -> JETZT AUF false DAMIT DIE LOGIK STIMMT!
const bool invertedPins[numKeys] = {false, false, false};

// Variablen für Entprellung (Debounce)
bool lastKeyStates[numKeys] = {false, false, false};
unsigned long lastDebounceTime[numKeys] = {0, 0, 0};
const unsigned long debounceDelay = 50; 

// ==========================================
// 4. DISPLAY CONFIGURATION
// ==========================================
TFT_eSPI tft = TFT_eSPI();

// ==========================================
// 5. CALLBACK: HÖRT AUF DASHBOARD-BEFEHLE
// ==========================================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Wenn der Ein-/Ausschaltbefehl für das Display kommt
  if (String(topic) == mqtt_switch_cmd_topic) {
    if (message == "ON") {
      Serial.println("--> HA Befehl empfangen: Display EIN");
      digitalWrite(38, HIGH); // Hintergrundbeleuchtung an
      mqttClient.publish(mqtt_switch_state_topic, "ON", true); // Status bestätigen
    } 
    else if (message == "OFF") {
      Serial.println("--> HA Befehl empfangen: Display AUS");
      digitalWrite(38, LOW);  // Hintergrundbeleuchtung aus (Strom sparen)
      mqttClient.publish(mqtt_switch_state_topic, "OFF", true); // Status bestätigen
    }
  }
}

// ==========================================
// HOME ASSISTANT AUTO-DISCOVERY LOGIK
// ==========================================
void setupMQTTDiscovery() {
  // 1. Discovery für die 3 Schlüssel
  for (int i = 0; i < numKeys; i++) {
    String configTopic = "homeassistant/binary_sensor/" + String(keyNames[i]) + "/config";
    stateTopics[i]     = "schluesselbrett/" + String(keyNames[i]) + "/state";

    DynamicJsonDocument doc(1024);
    doc["name"] = String(keyNames[i]);
    doc["stat_t"] = stateTopics[i];
    doc["unique_id"] = "schluesselbrett_" + String(keyNames[i]);
    doc["dev_cla"] = "presence"; 
    doc["payload_on"] = "ON";   
    doc["payload_off"] = "OFF"; 

    JsonObject device = doc.createNestedObject("device");
    device["identifiers"][0] = "schluesselbrett_sensoren_only";
    device["name"] = "Schluesselbrett Personen";
    device["model"] = "ESP32 Sensor-Modul";
    device["manufacturer"] = "Eigenbau";

    String payload;
    serializeJson(doc, payload);
    bool ok = mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    Serial.printf("HA Discovery fuer %s: %s\n", keyNames[i], ok ? "OK" : "FEHLGESCHLAGEN");
  }

  // 2. Discovery für den neuen Display-Schalter
  DynamicJsonDocument switchDoc(1024);
  switchDoc["name"] = "Display";
  switchDoc["stat_t"] = mqtt_switch_state_topic;
  switchDoc["cmd_t"] = mqtt_switch_cmd_topic;
  switchDoc["icon"] = "mdi:monitor";
  switchDoc["unique_id"] = "schluesselbrett_display_switch";
  switchDoc["payload_on"] = "ON";
  switchDoc["payload_off"] = "OFF";

  JsonObject swDevice = switchDoc.createNestedObject("device");
  swDevice["identifiers"][0] = "schluesselbrett_sensoren_only"; // Gleiche Geräte-Gruppe!

  String swPayload;
  serializeJson(switchDoc, swPayload);
  mqttClient.publish(mqtt_switch_config_topic, swPayload.c_str(), true);
  
  // Anfangszustand senden (Display leuchtet ja beim Start)
  mqttClient.publish(mqtt_switch_state_topic, "ON", true);
}

void forcePublishAllStates() {
  for(int i = 0; i < numKeys; i++) {
    bool raw = (digitalRead(reedPins[i]) == LOW);
    bool currentState = invertedPins[i] ? !raw : raw;
    mqttClient.publish(stateTopics[i].c_str(), currentState ? "ON" : "OFF", true);
    lastKeyStates[i] = currentState;
  }
}

// ==========================================
// SENSOR ABFRAGE
// ==========================================
void checkSensors() {
  for (int i = 0; i < numKeys; i++) {
    bool raw = (digitalRead(reedPins[i]) == LOW);
    bool reading = invertedPins[i] ? !raw : raw;

    if (reading != lastKeyStates[i]) {
      if ((millis() - lastDebounceTime[i]) > debounceDelay) {
        lastKeyStates[i] = reading;
        if (mqttClient.connected()) {
          mqttClient.publish(stateTopics[i].c_str(), reading ? "ON" : "OFF", true);
          Serial.printf("Status von %s geaendert auf: %s\n", keyNames[i], reading ? "ON" : "OFF");
        }
      }
      lastDebounceTime[i] = millis();
    }
  }
}

// ==========================================
// MQTT RECONNECT
// ==========================================
void reconnectMQTT() {
  if (mqttClient.connected()) return;
  
  Serial.print("Verbinde mit Mosquitto Broker...");
  String clientId = "ESP32Combined-";
  clientId += String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("erfolgreich verbunden!");
    
    // Sensoren & Display-Switch initialisieren
    setupMQTTDiscovery(); 
    forcePublishAllStates();
    
    // Auf Schaltbefehle von HA abonnieren
    mqttClient.subscribe(mqtt_switch_cmd_topic);
  } else {
    Serial.print("Fehler, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" Naechster Versuch folgt.");
  }
}

// ==========================================
// DISPLAY HILFSFUNKTIONEN
// ==========================================
String fixUmlaute(String s) {
  s.replace("ä", "ae"); s.replace("Ä", "Ae");
  s.replace("ö", "oe"); s.replace("Ö", "Oe");
  s.replace("ü", "ue"); s.replace("Ü", "Ue");
  s.replace("ß", "ss");
  return s;
}

uint16_t getCountdownColor(int cd) {
  if (cd < 0)  return TFT_DARKGREY;
  if (cd <= 2) return TFT_RED;
  if (cd <= 4) return TFT_YELLOW;
  return TFT_GREEN;
}

void drawRow(int yPos, String direction, int cd1, int cd2) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_GOLD);
  tft.drawString(direction, 8, yPos, 4);

  tft.setTextColor(TFT_ORANGE);
  tft.drawString("U6", 8, yPos + 26, 2);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(getCountdownColor(cd1));
  String z1 = (cd1 >= 0) ? String(cd1) : "--";
  tft.drawString(z1, 280, yPos, 7);

  tft.setTextColor(getCountdownColor(cd2));
  String z2 = (cd2 >= 0) ? String(cd2) : "--";
  tft.drawString(z2, 318, yPos + 8, 4);
}

bool fetchDepartures(const String& url, String& direction, int& cd1, int& cd2) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(client, url);

  int httpCode = http.GET();
  if (httpCode != 200) { http.end(); return false; }

  DynamicJsonDocument doc(30000);
  DeserializationError error = deserializeJson(
    doc, http.getStream(), DeserializationOption::NestingLimit(15)
  );
  http.end();
  
  if (error) {
    Serial.print("JSON Error: "); Serial.println(error.c_str());
    return false;
  }

  JsonObject lineObj = doc["data"]["monitors"][0]["lines"][0];
  direction = fixUmlaute(String(lineObj["towards"] | "Unbekannt"));
  JsonArray deps = lineObj["departures"]["departure"];
  cd1 = (deps.size() > 0) ? (int)deps[0]["departureTime"]["countdown"] : -1;
  cd2 = (deps.size() > 1) ? (int)deps[1]["departureTime"]["countdown"] : -1;
  return true;
}

// ==========================================
// SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  { uint32_t t = millis();
  while (!Serial && millis() - t < 4000) delay(10); }
  Serial.println("=== CLEAN SENSOR SETUP START ===");

  // Backlight Display als Output definieren und einschalten
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);

  // Hardware Sensor-Pins konfigurieren
  for (int i = 0; i < numKeys; i++) {
    stateTopics[i] = "schluesselbrett/" + String(keyNames[i]) + "/state";
    pinMode(reedPins[i], invertedPins[i] ? INPUT : INPUT_PULLUP);
  }

  // Display initialisieren
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Verbinde WiFi...", tft.width() / 2, tft.height() / 2, 2);

  // WiFi starten
  Serial.print("WiFi Verbindung herstellen...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
  }
  Serial.println(" OK");

  // MQTT Client Konfiguration
  mqttClient.setBufferSize(1024);
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback); // Registriert die Callback-Funktion!
  reconnectMQTT();

  tft.fillScreen(TFT_BLACK);
  Serial.println("=== SETUP FERTIG ===");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(3000);
    return;
  }

  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();

  // Sensoren abfragen
  checkSensors();

  // Wiener Linien abrufen
  String dir1, dir2;
  int cd1_1 = -1, cd1_2 = -1;
  int cd2_1 = -1, cd2_2 = -1;

  bool ok1 = fetchDepartures(urlFloridsdorf,  dir1, cd1_1, cd1_2);
  bool ok2 = fetchDepartures(urlSiebenhirten, dir2, cd2_1, cd2_2);

  // Display zeichnen
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Jaegerstrasse", tft.width() / 2, 2, 2);
  tft.drawFastHLine(6, 20, tft.width() - 12, 0x3186);

  if (ok1) {
    drawRow(28, dir1, cd1_1, cd1_2);
  } else {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Floridsdorf - keine Daten", 8, 28, 2);
  }
  
  tft.drawFastHLine(6, 85, tft.width() - 12, 0x1863);

  if (ok2) {
    drawRow(90, dir2, cd2_1, cd2_2);
  } else {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Siebenhirten - keine Daten", 8, 90, 2);
  }

  // 30 Sekunden blockierungsfreie Wartezeit (Sensoren & MQTT bleiben voll aktiv!)
  unsigned long waitUntil = millis() + 30000;
  while (millis() < waitUntil) {
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop();
    checkSensors(); 
    delay(50);
  }
}x