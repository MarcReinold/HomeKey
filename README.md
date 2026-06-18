# HomeKey

Smart-Schlüsselbrett mit Wiener-Linien-Live-Anzeige, Reed-Sensorik und Home-Assistant/MQTT-Integration.

## Überblick

HomeKey ist ein selbst entwickeltes "smartes" Schlüsselbrett für eine Wohngemeinschaft (Kilian, Marc, Patrik). Es kombiniert:

- Eine Live-Abfahrtsanzeige der Wiener Linien (U6) für zwei Haltestellen (Floridsdorf, Siebenhirten) auf einem **Lilygo T-Display S3** (ESP32-S3, ST7789 TFT, 8-Bit Parallel).
- Drei **Reed-Kontakt-Sensoren**, die erkennen, ob der Schlüssel von Kilian, Marc bzw. Patrik am Brett hängt.
- Eine **Home-Assistant/MQTT-Infrastruktur** (Docker), die die Sensordaten empfängt, auswertet und in einem Dashboard visualisiert – inklusive einer MQTT-gesteuerten Ein/Aus-Schaltung für das Display.

Eine ausführliche Projektdokumentation (Hardware, Software-Architektur, Entwicklungs-Historie, Troubleshooting) liegt als `HomeKey_Projektdokumentation.docx` im Hauptordner.

## Repo-Inhalt

| Datei | Beschreibung |
|---|---|
| `Main_code.ino` | Aktueller produktiver ESP32-Sketch: Display, Reed-Sensoren, MQTT/Home-Assistant-Discovery, Display-Ein/Aus-Schalter |
| `User_Setup.h` | TFT_eSPI-Pinkonfiguration für das Lilygo T-Display S3 (in `Dokumente/Arduino/libraries/TFT_eSPI/User_Setup.h` einfügen) |
| `docker-compose.yml` | Docker-Compose-Setup für Home Assistant, Mosquitto (MQTT-Broker), ESPHome und einen Nginx-Webserver |
| `mosquitto/mosquitto.conf` | Mosquitto-Broker-Konfiguration (Listener 1883, anonyme Verbindungen erlaubt) |

## Hardware

- Lilygo T-Display S3 (ESP32-S3)
- 3× Reed-Kontakt-Sensor (Kilian: GPIO 10, Marc: GPIO 12, Patrik: GPIO 11 – KY-021-Modul)
- Geplant/in Arbeit: WS2812B-LED-Streifen (6 LEDs Anwesenheitsanzeige auf GPIO 2, 35 LEDs Abfahrtszeiten-Farbcodierung auf GPIO 16, externes 5V-Netzteil)

## Setup

1. `docker-compose up -d` im Ordner mit `docker-compose.yml` ausführen → startet Home Assistant (Port 8123), Mosquitto (Port 1883/9001), ESPHome (Port 6052), Nginx (Port 8080).
2. `User_Setup.h` in die TFT_eSPI-Bibliothek kopieren.
3. In `Main_code.ino` WLAN-Zugangsdaten (`ssid`, `password`) und MQTT-Broker-IP (`mqtt_server`) an das eigene Netzwerk anpassen.
4. Sketch auf das Lilygo T-Display S3 hochladen (Arduino IDE, ESP32-S3-Board ausgewählt).
5. In Home Assistant erscheinen die Sensoren (`binary_sensor.kilian/marc/patrik`) und der Display-Schalter automatisch per MQTT-Auto-Discovery.

## Status

- Wiener-Linien-Anzeige, Reed-Sensoren (Kilian/Marc) und MQTT/Dashboard-Integration funktionieren.
- Patrik-Sensor (KY-021): Nachlöten von 5V auf 3,3V ausständig.
- LED-Streifen: Verkabelung/Firmware-Integration in Arbeit, noch nicht in `Main_code.ino` zusammengeführt.

Details siehe `HomeKey_Projektdokumentation.docx`.
