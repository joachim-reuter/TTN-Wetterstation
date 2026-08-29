# TTN Wetterstation mit ESP32 und E-Paper-Display

## 1. Projektbeschreibung

Dieses Projekt realisiert eine zentrale Wetter- und Statusanzeige auf Basis eines ESP32.

Die Messwerte werden von mehreren LoRaWAN-Sensoren erfasst und über ein LoRaWAN-Gateway an **The Things Network (TTN)** übertragen. Der ESP32 ruft die Daten anschließend über die MQTT-Schnittstelle von TTN ab.

Die empfangenen Messwerte werden auf einem **7,5"-E-Paper-Display** dargestellt.

Zusätzlich ruft der ESP32 regelmäßig eine Wettervorhersage für Halle über die OpenWeatherMap-API ab und stellt diese ebenfalls auf dem Display dar.

---

## 2. Systemübersicht

Das Gesamtsystem besteht aus folgenden Komponenten:

1. LoRaWAN-Sensor 15-01
2. LoRaWAN-Sensor 15-02
3. LoRaWAN-Gateway
4. The Things Network (TTN)
5. WLAN-Netzwerk
6. ESP32-Wetterstation
7. 7,5"-E-Paper-Display
8. OpenWeatherMap Forecast API

### Datenweg der Messwerte

```text
Sensor 15-01
     │
     │ LoRaWAN
     ▼
LoRaWAN Gateway
     │
     │ Internet
     ▼
The Things Network
     │
     │ MQTT/TLS
     ▼
ESP32
     │
     ▼
E-Paper Display
```

### Datenweg der Wettervorhersage

```text
ESP32
  │
  │ HTTP
  ▼
OpenWeatherMap
  │
  │ JSON
  ▼
ESP32
  │
  ▼
E-Paper Display
```

---

## 3. LoRaWAN-Sensoren

### Sensor 15-01

Das Gerät `15-01` liefert folgende Werte:

* `TempC_SHT` – Temperatur des SHT-Sensors
* `Hum_SHT` – Luftfeuchtigkeit
* `TempC_DS` – Außentemperatur

Der ESP32 übernimmt diese Werte in die Variablen:

```text
temp_raum1
hum_raum1
temp_aussen
```

Zusätzlich wird der Zeitpunkt des letzten empfangenen Datensatzes gespeichert.

### Sensor 15-02

Das Gerät `15-02` liefert:

* `temperature`
* `distance_m`

Die Temperatur wird im Programm zusätzlich um 4 °C korrigiert:

```text
temp_raum2 = temperature - 4
```

Die Distanz wird in `distance_m` gespeichert.

---

## 4. Verbindung zu The Things Network

Der ESP32 verwendet die MQTT-Schnittstelle von TTN.

Server:

```text
eu1.cloud.thethings.network
```

Port:

```text
8883
```

Die Verbindung erfolgt über `WiFiClientSecure`.

Für MQTT wird die Bibliothek `PubSubClient` verwendet.

Die verwendeten Komponenten sind:

```cpp
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
```

Damit übernimmt der ESP32 folgende Aufgaben:

* Verbindung mit dem WLAN
* Aufbau der sicheren MQTT-Verbindung
* Anmeldung bei TTN
* Subscription auf die Uplink-Nachrichten
* Empfang der JSON-Daten
* Dekodierung der Nutzdaten
* Aktualisierung der Displaydaten

---

## 5. MQTT-Datenfluss

Der ESP32 abonniert folgendes MQTT-Thema:

```text
v3/015@ttn/devices/+/up
```

Das `+` ist ein MQTT-Wildcard und ermöglicht den Empfang der Uplink-Nachrichten verschiedener Geräte innerhalb der Anwendung.

Der MQTT-Datenfluss ist damit:

```text
LoRaWAN Sensor
      │
      ▼
   Gateway
      │
      ▼
     TTN
      │
      ▼
 MQTT Broker
      │
      │ Topic:
      │ v3/015@ttn/devices/+/up
      ▼
    ESP32
```

---

## 6. Verarbeitung der TTN-Nachricht

Bei Eingang einer MQTT-Nachricht wird die Funktion

```cpp
callback()
```

aufgerufen.

Zunächst wird die empfangene JSON-Nachricht verarbeitet:

```cpp
StaticJsonDocument<2048> doc;
deserializeJson(doc, payload, length);
```

Anschließend wird der Bereich

```text
uplink_message
    └── decoded_payload
```

ausgelesen.

Zusätzlich wird die Device-ID aus

```text
end_device_ids.device_id
```

ermittelt.

Der Empfangszeitpunkt wird aus

```text
received_at
```

übernommen.

---

## 7. Zuordnung der Sensordaten

Der ESP32 entscheidet anhand der Device-ID, welcher Sensor die Daten geliefert hat.

### Gerät 15-01

```text
Device ID = 15-01
        │
        ├── TempC_SHT → temp_raum1
        ├── Hum_SHT   → hum_raum1
        └── TempC_DS  → temp_aussen
```

### Gerät 15-02

```text
Device ID = 15-02
        │
        ├── temperature → temp_raum2
        │                   └── -4 °C Korrektur
        │
        └── distance_m → distance_m
```

Nach dem Empfang wird

```cpp
updateNeeded = true;
```

gesetzt.

Dadurch wird im Hauptprogramm eine Aktualisierung des Displays ausgelöst.

---

## 8. E-Paper-Display

Das Projekt verwendet ein 7,5"-E-Paper-Display.

Die Displaybibliotheken sind:

```cpp
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "fonts.h"
```

Der Displaypuffer wird dynamisch im RAM angelegt.

Beim Start wird geprüft, ob ausreichend Speicher vorhanden ist. Bei einem Fehler wird die Programmausführung angehalten.

---

## 9. Anzeige der Messwerte

Auf der linken Seite des Displays werden die aktuellen Sensordaten dargestellt:

```text
TTN WETTERSTATION
────────────────────────

Aussen        : xx.x C

Schlafzimmer  : xx.x C

Innen         : xx.x C

Feuchte       : xx %

Distanz       : xx.xx m
```

Zusätzlich werden die letzten Upload-Zeitpunkte der beiden Sensoren angezeigt.

Damit lässt sich erkennen, ob von den einzelnen LoRaWAN-Geräten noch aktuelle Daten eintreffen.

---

## 10. Belegt-/Frei-Anzeige

Die Distanz des Sensors 15-02 wird zur Ermittlung eines Status verwendet.

Die Schaltschwelle beträgt:

```text
1,3 m
```

Logik:

```text
distance_m < 1,3 m
        │
        ├── JA → STATUS : BESETZT
        │
        └── NEIN → STATUS : FREI
```

Damit kann der Distanzsensor beispielsweise zur Überwachung eines Bereiches oder Stellplatzes verwendet werden.

---

## 11. WLAN- und MQTT-Status

Das Display zeigt außerdem den Zustand der Netzwerkverbindung.

Mögliche Anzeigen:

```text
WLAN OK
WLAN FEHLER
```

und

```text
MQTT OK
MQTT FEHLER
```

Damit können Netzwerk- und TTN-Probleme direkt am Gerät erkannt werden.

---

## 12. MQTT-Wiederverbindung

Sollte die MQTT-Verbindung unterbrochen werden, erkennt dies die Hauptschleife:

```cpp
if (!client.connected())
```

Anschließend wird `reconnect()` aufgerufen.

Der ESP32 versucht solange eine Verbindung aufzubauen, bis diese erfolgreich hergestellt wurde.

Nach erfolgreicher Verbindung wird das MQTT-Thema erneut abonniert.

---

## 13. Wettervorhersage

Neben den LoRaWAN-Daten verwendet das System eine zweite Datenquelle.

Der ESP32 ruft die OpenWeatherMap Forecast API auf.

Abgefragt wird:

```text
Halle, Deutschland
```

Die Wetterdaten werden als JSON empfangen und anschließend ausgewertet.

Die Forecast-Daten werden in einer Struktur gespeichert:

```text
ForecastDay
 ├── day
 ├── tempMin
 ├── tempMax
 ├── weather
 └── icon
```

Es werden drei Tage verarbeitet.

---

## 14. Aktualisierungsintervall der Wettervorhersage

Die Wettervorhersage wird alle

```text
1.800.000 ms
```

erneut abgefragt.

Das entspricht:

```text
30 Minuten
```

Die Prüfung erfolgt in der Hauptschleife:

```text
millis() - lastForecastUpdate > 1800000
```

Damit wird die Wetter-API nicht bei jedem Durchlauf des Programms aufgerufen.

---

## 15. Wetterdarstellung

Für die drei Tage werden angezeigt:

* Datum
* minimale Temperatur
* maximale Temperatur
* Wetterbeschreibung
* Wettersymbol

Das Programm interpretiert die von OpenWeatherMap gelieferten Icon-Codes.

Zuordnung:

```text
01 → Sonne
02 → teilweise bewölkt
03/04 → Wolken
09/10 → Regen
11 → Gewitter
13 → Schnee
```

Die Symbole werden direkt durch Zeichenfunktionen im ESP32 erzeugt.

---

## 16. Sonderzeichen

Da die verwendeten Displayfonts nicht direkt mit deutschen Umlauten arbeiten, werden diese vor der Anzeige ersetzt.

Beispiele:

```text
ä → ae
ö → oe
ü → ue
ß → ss
```

Beispiel:

```text
mäßig bewölkt
```

wird zu:

```text
maessig bewoelkt
```

---

## 17. Programmablauf

### Start

Beim Einschalten führt der ESP32 folgende Schritte aus:

```text
START
  │
  ▼
Serielle Schnittstelle starten
  │
  ▼
E-Paper initialisieren
  │
  ▼
Display löschen
  │
  ▼
Displaypuffer anlegen
  │
  ▼
WLAN verbinden
  │
  ▼
MQTT konfigurieren
  │
  ▼
Forecast abrufen
  │
  ▼
Display aktualisieren
  │
  ▼
Hauptschleife
```

---

## 18. Hauptschleife

Im laufenden Betrieb arbeitet der ESP32 im Wesentlichen nach folgendem Schema:

```text
┌─────────────────────────────┐
│            LOOP             │
└──────────────┬──────────────┘
               │
               ▼
       MQTT verbunden?
          /       \
        Nein       Ja
         │          │
         ▼          ▼
     reconnect   client.loop()
         │          │
         └────┬─────┘
              │
              ▼
       updateNeeded?
          /       \
        Nein       Ja
         │          │
         │          ▼
         │     Display Update
         │          │
         └────┬─────┘
              │
              ▼
     30 Minuten vorbei?
          /       \
        Nein       Ja
         │          │
         │          ▼
         │    Forecast abrufen
         │          │
         └────┬─────┘
              │
              ▼
            LOOP
```

---

## 19. Softwarekomponenten

### ESP32

Zentrale Verarbeitungseinheit des Projektes.

Aufgaben:

* WLAN
* MQTT
* JSON-Verarbeitung
* Datenaufbereitung
* Wetterdatenabruf
* Displaysteuerung

### PubSubClient

Wird für die MQTT-Kommunikation mit TTN verwendet.

### ArduinoJson

Wird zur Verarbeitung der von TTN und OpenWeatherMap gelieferten JSON-Daten verwendet.

### HTTPClient

Wird für den Abruf der Wettervorhersage verwendet.

### E-Paper-Treiber

Die E-Paper-Funktionen werden über die bereitgestellten Displaybibliotheken realisiert.

---

## 20. Datenfluss Gesamtübersicht

```text
                   LoRaWAN
              ┌───────────────┐
              │               │
       ┌──────▼──────┐ ┌──────▼──────┐
       │ Gerät 15-01 │ │ Gerät 15-02 │
       │             │ │             │
       │ Temp        │ │ Temperatur  │
       │ Feuchte     │ │ Distanz     │
       │ Außen       │ │             │
       └──────┬──────┘ └──────┬──────┘
              │               │
              └───────┬───────┘
                      │
                      ▼
              ┌───────────────┐
              │ LoRaWAN       │
              │ Gateway       │
              └───────┬───────┘
                      │
                   Internet
                      │
                      ▼
              ┌───────────────┐
              │     TTN       │
              │               │
              │ LoRaWAN       │
              │ Network       │
              │ Server        │
              │               │
              │ MQTT Broker   │
              └───────┬───────┘
                      │
                 MQTT / TLS
                   Port 8883
                      │
                      ▼
              ┌───────────────┐
              │     ESP32     │
              │               │
              │ MQTT          │
              │ JSON          │
              │ Verarbeitung  │
              └───────┬───────┘
                      │
             ┌────────┴────────┐
             │                 │
             ▼                 ▼
      ┌─────────────┐   ┌─────────────┐
      │ E-Paper     │   │ OpenWeather │
      │ Display     │   │ API         │
      └─────────────┘   └──────┬──────┘
                                │
                                │ Wetterdaten
                                ▼
                           zurück zum
                             ESP32
                                │
                                ▼
                           E-Paper
```

---

## 21. Wichtige Projektparameter

| Parameter               | Wert                        |
| ----------------------- | --------------------------- |
| Mikrocontroller         | ESP32                       |
| LoRaWAN-Netz            | The Things Network          |
| MQTT-Server             | eu1.cloud.thethings.network |
| MQTT-Port               | 8883                        |
| MQTT-Protokoll          | MQTT über TLS               |
| Sensor 1                | 15-01                       |
| Sensor 2                | 15-02                       |
| Display                 | 7,5" E-Paper                |
| Wetterdienst            | OpenWeatherMap              |
| Wetterort               | Halle, Deutschland          |
| Forecast                | 3 Tage                      |
| Forecast-Aktualisierung | 30 Minuten                  |
| Belegt-Schwelle         | 1,3 m                       |
| Serielle Schnittstelle  | 115200 Baud                 |

---



## 22. Zusammenfassung

Die Wetterstation verbindet mehrere Technologien zu einem zentralen Informationssystem:

```text
LoRaWAN
   ↓
Gateway
   ↓
The Things Network
   ↓
MQTT
   ↓
ESP32
   ↓
E-Paper
```

Parallel dazu:

```text
OpenWeatherMap
      ↓
    HTTP
      ↓
    ESP32
      ↓
   E-Paper
```

Der ESP32 fungiert damit als **zentrale Anzeige- und Verarbeitungseinheit**. Die eigentliche Sensorübertragung erfolgt vollständig über LoRaWAN und TTN. Die Wettervorhersage wird unabhängig davon über das Internet abgerufen.

Die Architektur ermöglicht es, weitere LoRaWAN-Geräte relativ einfach in das System aufzunehmen, sofern deren Daten über das TTN-MQTT-Interface verfügbar sind und im ESP32 entsprechend verarbeitet werden.
