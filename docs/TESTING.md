# Umfassende Test-Anleitung

## Phase 0: Vorbereitung

### Anforderungen
- 2x ESP32 boards (oder 1x für Standalone), bzw. 2x für Master-Slave
- 2x USB Kabel
- Computer mit PlatformIO
- MQTT Broker (Mosquitto, Home Assistant, or Docker)
- SH1106 OLED 128x64 (1.3 inch, I2C) fur optionalen Hardware-Check
- Oscilloscope (optional aber empfohlen für Qualitäts-Tests)
- oder Multimeter mit AC-Modus

### Test-Umgebung Setup

```bash
# 1. MQTT Broker starten (Docker)
docker run -d --name mosquitto -p 1883:1883 eclipse-mosquitto

# 2. Repo klonen & vorbereiten
cd myenergi_CT-Clamp-Simulator

# 3. config.h anpassen (WiFi + MQTT)
nano include/config.h
# WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER

# 4. Bauen
pio run -e esp32-master
pio run -e esp32-slave
```

---

## Test 1: Standalone Single Board (5 Min)

### Szenario
Ein einzelnes ESP32-Board, alles lokal.

### Setup
```bash
# Bauen & Hochladen
pio run -e esp32 -t upload

# Monitor öffnen
pio run -e esp32 -t monitor
```

### Erwartetes Output
```
=== Myenergi Harvi CT Simulator ===
Mode: STANDALONE (All phases A,B,C)
Initializing...
✓ Signal generator initialized
✓ MQTT handler initialized
Connecting to WiFi: MyHome
✓ WiFi connected!
  IP: 192.168.1.50
Connecting to MQTT: 192.168.1.100:1883
✓ MQTT connected!
✓ Setup complete!

--- Status Report ---
WiFi: ✓ Connected
MQTT: ✓ Connected
Phase A: 0.00A (DAC: 128)
Phase B: 0.00A (DAC: 128)
Phase C: 0.00A (DAC: 128)
```

### Test-Aktionen

**1. MQTT Verbindung testen:**
```bash
mosquitto_pub -h 192.168.1.100 -t /esp32CTSimulator/PhaseA_Amp -m "25.5"
```

Erwartet in Monitor:
```
📡 MQTT [/esp32CTSimulator/PhaseA_Amp]: 25.50
Phase A: 25.50A (DAC: 195)
```

**2. Alle Phasen testen:**
```bash
mosquitto_pub -h 192.168.1.100 -t /esp32CTSimulator/PhaseA_Amp -m "25"
mosquitto_pub -h 192.168.1.100 -t /esp32CTSimulator/PhaseB_Amp -m "23"
mosquitto_pub -h 192.168.1.100 -t /esp32CTSimulator/PhaseC_Amp -m "24"
```

**3. Oszilloskop-Messung (optional):**
```
CH1 → GPIO25: Sollte 50Hz Sinus zeigen, ~1.6V Offset
CH2 → GPIO26: Sollte 50Hz Sinus zeigen, 120° Phase-Versatz
CH3 → GPIO32: Sollte 50Hz Sinus zeigen, 240° Phase-Versatz
```

### ✓ Test Bestanden wenn:
- [ ] MQTT Verbindung etabliert
- [ ] Boot-Nachricht sichtbar
- [ ] MQTT-Werte ändern DAC-Werte
- [ ] OLED zeigt A/B/C Werte konsistent zu den MQTT-Payloads (falls aktiviert)
- [ ] Oszilloskop zeigt saubere 50Hz Sinuswelle

---

## Test 2: Master-Slave Verbindung (10 Min)

### Szenario
Zwei ESP32s, kommunizieren per UART.

### Hardware-Setup

```
Master ESP32                Slave ESP32
GPIO17 (TX) ─────────────► GPIO16 (RX)
GPIO16 (RX) ◄───────────── GPIO17 (TX)
GND ──────────────────────── GND
(Shielded Cable, <3m)
```

### Software-Setup

**Master programmieren:**
```bash
pio run -e esp32-master -t upload
```

**Slave programmieren (separates USB):**
```bash
pio run -e esp32-slave -t upload
```

### Dual-Monitoring

**Terminal 1 - Master:**
```bash
pio run -e esp32-master -t monitor
```

**Terminal 2 - Slave (neues Terminal):**
```bash
pio run -e esp32-slave -t monitor
```

### Erwartetes Output

**Master (Terminal 1):**
```
=== Myenergi Harvi CT Simulator ===
Mode: MASTER (Phase A + MQTT)
...
✓ WiFi connected!
✓ MQTT connected!
Slave UART initialized: 460800 baud
✓ Setup complete!

--- Status Report ---
WiFi: ✓ Connected
MQTT: ✓ Connected
Slave: ✓ Connected
Phase A: 0.00A (DAC: 128)
```

**Slave (Terminal 2):**
```
=== Myenergi Harvi CT Simulator ===
Mode: SLAVE (Phase B, C via UART)
...
Slave UART listening: 460800 baud
✓ Setup complete!

--- Status Report ---
Master: ✓ Connected
Phase B: 0.00A (DAC: 128)
Phase C: 0.00A (DAC: 128)
```

### Test-Aktionen

**1. UART Verbindung verifizieren:**

Sollte innerhalb 5 Sekunden zeigen:
```
Master: "Slave: ✓ Connected"
Slave:  "Master: ✓ Connected"
```

Wenn nicht:
- Kabel neu anschließen
- Baud-Rate checken
- TX/RX nicht vertauscht?

**2. Daten durch MQTT senden:**

```bash
mosquitto_pub -h 192.168.1.100 -t /esp32CTSimulator/PhaseA_Amp -m "30"
mosquitto_pub -h 192.168.1.100 -t /esp32CTSimulator/PhaseB_Amp -m "25"
mosquitto_pub -h 192.168.1.100 -t /esp32CTSimulator/PhaseC_Amp -m "28"
```

**Master Terminal sollte zeigen:**
```
📡 MQTT [/esp32CTSimulator/PhaseA_Amp]: 30.00
📡 MQTT [/esp32CTSimulator/PhaseB_Amp]: 25.00
📡 MQTT [/esp32CTSimulator/PhaseC_Amp]: 28.00

→ Slave: B=25.00A, C=28.00A
```

**Slave Terminal sollte zeigen:**
```
Master: ✓ Connected
Phase B: 25.00A (DAC: 180)
Phase C: 28.00A (DAC: 194)
```

### ✓ Test Bestanden wenn:
- [ ] Beide Boards verbinden schnell
- [ ] UART Verbindung wird erkannt
- [ ] MQTT-Werte kommen auf Master an
- [ ] Slave empfängt Phase B/C via UART
- [ ] DAC-Werte aktualisieren sich richtig

---

## Test 3: Oszilloskop-Analyse (15 Min)

### Equipment
- Digitales Oszilloskop (100MHz+ empfohlen)
- 3x Probe & Kabel
- 2x Alligator Clips (für GND)

### Setup

```
Master ESP32:
  GPIO25 → CH1 Probe
  GND    → GND Clip

Slave ESP32:
  GPIO25 → CH3 Probe
  GPIO26 → CH2 Probe
  GND    → GND Clip (gemeinsam!)
```

### Messungen

**1. DC Offset überprüfen (keine MQTT-Werte):**

```
Erwartet:
  CH1: 1.65V ± 0.05V
  CH2: 1.65V ± 0.05V
  CH3: 1.65V ± 0.05V

Wenn nicht:
  - Zu hoch (>2V): Kalibration Problem
  - Zu niedrig (<1.5V): Software-Fehler
  - Floating (wechselt): GPIO nicht richtig initialisiert
```

**2. AC Amplitudе (mit 25A MQTT-Signal):**

```
Sende: mosquitto_pub -h ... -t /esp32CTSimulator/PhaseA_Amp -m "25"

Erwartet:
  [Peak-to-Peak Spannung]
  Min: 1.65V - 0.8V = 0.85V
  Max: 1.65V + 0.8V = 2.45V
  Peak-to-Peak: ~1.6V

  Für 50A:
  Min: 1.65V - 1.6V = 0.05V (~0V)
  Max: 1.65V + 1.6V = 3.25V (~3.3V)
  Peak-to-Peak: ~3.2V

Wenn Signal zu klein:
  - Gain in config.h erhöhen
  - MQTT-Wert höher halten
  - DAC-Pin überprüfen
```

**3. Frequenzmessung:**

```
Oszi: Frequency Measurement → CH1

Erwartet:
  49.9 - 50.1 Hz (EU)
  oder 59.9 - 60.1 Hz (US)

Wenn nicht:
  - Prozessortakt prüfen
  - UPDATE_INTERVAL in config.h überprüfen
```

**4. Phase-Versatz (alle 3 Phasen mit gleichem Wert):**

```
Sende: mosquitto_pub -h ... -t /esp32CTSimulator/PhaseA_Amp -m "50"
  mosquitto_pub -h ... -t /esp32CTSimulator/PhaseB_Amp -m "50"
  mosquitto_pub -h ... -t /esp32CTSimulator/PhaseC_Amp -m "50"

Oszi-Einstellung:
  - Time base: 5ms/div (full 20ms sichtbar)
  - Trigger: CH1 Rising Edge
  
Erwartet:
  CH1 (Phase A): Referenz
  CH2 (Phase B): 120° später = 6.67ms später
  CH3 (Phase C): 240° später = 13.33ms später

Messung mit Oszi Cursor:
  Phase A zero-crossing bei 0ms
  Phase B zero-crossing bei ~6.67ms
  Phase C zero-crossing bei ~13.33ms

Phase error sollte < 1ms sein!
```

**5. Rausch & Verzerrung:**

```
Fall: 100A Signal (Maximum)

Erwartet:
  - Smooth Sinus-Welle
  - THD < 5% (Total Harmonic Distortion)
  - Rausch-Amplitude < 50mV peak

Wenn Verzerrung sichtbar:
  - Shielded Kabel verwenden
  - Kabel kürzer machen
  - Filter (0.1µF) über DAC Ausgabe
  - PWM Träger-Frequenz überprüfen (falls PWM)
```

### ✓ Test Bestanden wenn:
- [ ] DC Offset ±50mV genau
- [ ] AC Amplitude passt zu Signal-Wert
- [ ] Frequenz 50Hz ± 0.1Hz
- [ ] Phase-Versatz 120° ± 1°
- [ ] Rausch minimal, Signal sauberer Sinus

---

## Test 4: Signal-Integrität (20 Min)

### Szenario
Verschiedene Strompegel testen und Profile aufzeichnen.

### Test-Script

Erstelle `test_profiles.py`:

```python
#!/usr/bin/env python3
import paho.mqtt.client as mqtt
import time
import sys

broker = "192.168.1.100"
test_profiles = {
    "No Load": {"a": 0, "b": 0, "c": 0, "duration": 5},
    "Light Load": {"a": 10, "b": 10, "c": 10, "duration": 5},
    "Medium Load": {"a": 30, "b": 28, "c": 32, "duration": 5},
    "Heavy Load": {"a": 70, "b": 68, "c": 72, "duration": 5},
    "Max Load": {"a": 100, "b": 95, "c": 100, "duration": 5},
    "Ramp Up": {"ramp": True, "duration": 30},
}

client = mqtt.Client()
client.connect(broker, 1883)

def publish_values(a, b, c):
    client.publish("/esp32CTSimulator/PhaseA_Amp", f"{a:.1f}")
    client.publish("/esp32CTSimulator/PhaseB_Amp", f"{b:.1f}")
    client.publish("/esp32CTSimulator/PhaseC_Amp", f"{c:.1f}")
    print(f"Published: A={a}A, B={b}A, C={c}A")

print("Starting Test Profiles...")

for name, profile in test_profiles.items():
    print(f"\n▶ Test: {name}")
    
    if "ramp" in profile and profile["ramp"]:
        # Ramp from 0 to 100A over duration
        for i in range(100):
            current = i
            publish_values(current, current*0.9, current*0.8)
            time.sleep(profile["duration"]/100)
    else:
        # Constant values
        publish_values(profile["a"], profile["b"], profile["c"])
        time.sleep(profile["duration"])
    
    publish_values(0, 0, 0)  # Reset
    print(f"✓ Complete")

client.disconnect()
print("\n✓ All profiles completed!")
```

**Ausführen:**
```bash
python3 test_profiles.py
```

### Beobachtungen während Test

Öffne 2 Terminals:
```
# Terminal 1: Master Monitor
pio run -e esp32-master -t monitor | grep "Phase"

# Terminal 2: Slave Monitor  
pio run -e esp32-slave -t monitor | grep "Phase"
```

**Erwartet:**
```
Master Monitor:
Phase A: 10.00A (DAC: 135)
Phase A: 30.00A (DAC: 160)
Phase A: 70.00A (DAC: 224)
Phase A: 100.00A (DAC: 255)

Slave Monitor (3-5ms Verzögerung erwartet):
Phase B: 10.00A (DAC: 135)
Phase B: 30.00A (DAC: 160)
...
```

### Daten-Validierung

Mit Oszi aufzeichnen (falls möglich):
- Amplitude linear mit Strom
- Frequency stabil 50Hz
- Phase-Versatz konstant 120°
- Keine Glitches oder Dropout

---

## Test 5: MQTT Robustheit (10 Min)

### Scenario
MQTT Broker simulierenabschalten, Reconnect testen.

### Test-Schritte

**1. Broker stoppen:**
```bash
# Falls Docker:
docker stop mosquitto

# Monitor sollte zeigen:
# ⚠ MQTT disconnected, attempting to reconnect...
# ✗ MQTT connection failed!
# (wiederholt sich alle 5 Sekunden)
```

**2. Broker wieder starten:**
```bash
docker start mosquitto

# Monitor sollte zeigen:
# ✓ MQTT connected!
# Slave: ✓ Connected
```

**3. MQTT Nachricht senden:**
```bash
mosquitto_pub -h 192.168.1.100 -t /esp32CTSimulator/PhaseA_Amp -m "40"
# Sollte funktonieren
```

### ✓ Test Bestanden wenn:
- [ ] Disconnect erkannt (<6s)
- [ ] Reconnect automatisch
- [ ] Nach Reconnect wieder Daten empfangen
- [ ] Slave bleibt verbunden (kennt nur UART!)

---

## Test 6: Power Consumption (Optional, 15 Min)

### Equipment
- USB Power Meter (benötigt USB Adapter)
- oder Multimeter mit Ampere-Funktion

### Messung

**Setup:**
```
USB Power Supply → USB Power Meter → Master ESP32

Alternativ:
USB Power Supply → [Break GND] → Multimeter (mA Bereich) → Master ESP32 GND
```

### Messpunkte

```
Condition                    Master    Slave    Total
─────────────────────────────────────────────────────
Both idle (0A signal)        60mA      35mA     95mA
MQTT active (50A signal)     85mA      40mA     125mA
Heavy load (100A, 3P)        95mA      45mA     140mA

Slave + Deep Sleep (if enabled)
  - Enabled                  60mA      8mA      68mA
  - Disabled                 60mA      35mA     95mA
```

### Berechnung

```
Daily consumption (24h, durchschn. 50A):
  • Master: 85mA × 24h = 2040 mAh/Tag
  • Slave: 40mA × 24h = 960 mAh/Tag
  
  Mit 5V:
  • Master: 2040 × 5V = 10.2 Wh/Tag
  • Slave: 960 × 5V = 4.8 Wh/Tag
  • Total: 15 Wh/Tag

  Batterie (5000mAh 5V):
  • Slave kann ~5 Tage auf Batterie laufen!
```

---

## Test 7: Harvi/Zappi Integration (30 Min, Real-World!)

### Vorbereitung

```
1. Harvi/Zappi physisch herrichten
2. Genaue CT-Positionen notieren (welche Phase an welchem CT?)
3. Test-Strommesswerte verfügbar haben
```

### Setup

```
Harvi/Zappi:
  Phase A CT Input ← Master GPIO25 DAC
  Phase B CT Input ← Slave GPIO25 DAC
  Phase C CT Input ← Slave GPIO26 DAC
  GND (common)

Computer:
  MQTT Broker läuft
  Master ESP32 verbunden
  Slave ESP32 verbunden
```

### Messungen auf Harvi Display

**Test 1: Null-Punkt Kalibrierung**
```
MQTT: A=0, B=0, C=0

Erwartet auf Harvi:
  All phases: 0.0A or sehr nahe 0

Problem wenn:
  • Nicht Null: Offset-Fehler
  • Zittern: Rausch
```

**Test 2: Kleine Last**
```
MQTT: A=5, B=5, C=5

Erwartet auf Harvi:
  Phase A: 4.8-5.2A
  Phase B: 4.8-5.2A
  Phase C: 4.8-5.2A
```

**Test 3: Normal Betrieb**
```
MQTT: A=25, B=23, C=26

Erwartet auf Harvi:
  Phase A: 24.5-25.5A
  Phase B: 22.5-23.5A
  Phase C: 25.5-26.5A
```

**Test 4: Volle Last**
```
MQTT: A=100, B=95, C=100

Erwartet auf Harvi:
  Phase A: 95-100A
  Phase B: 90-95A
  Phase C: 95-100A
```

### If Harvi zeigt falsche Werte

```
Problem: Zu hoch (2x Wert)
  → CT Polarität umkehren? Nein, sollte nicht relevant sein
  → Kalibrierungs-Konstante in Harvi überprüfen
  → Signal-Amplitude mit Oszi überprüfen

Problem: Zu niedrig (0.5x Wert)
  → DAC nicht auf Maximum gehen?
  → Signal-Amplitude zu klein

Problem: Zitter/Rausch
  → 0.1µF Filter über DAC Ausgabe hinzufügen
  → Shielded Kabel verwenden
  → Kabel kürzer machen

Problem: False Phase Detection
  → Phase-Versatz korrekt? (120°)
  → Frequenz Genauigkeit? (50Hz)
```

### ✓ Test Bestanden wenn:
- [ ] Harvi zeigt Werte sehr nahe MQTT Input
- [ ] Fehler < 1%
- [ ] Keine Rausch-Spitzen
- [ ] Stabil über mehrere Minuten

---

## Test 8: Langzeit-Stabilität (1 Stunde, Optional)

### Scenario
System läuft über längere Zeit, alles sollte stabil bleiben.

### Setup

```bash
# Test mit Python-Loop über 1 Stunde

cat > test_longterm.py << 'EOF'
import paho.mqtt.client as mqtt
import time
import random

client = mqtt.Client()
client.connect("192.168.1.100", 1883)

print("Starting 1-hour stability test...")
start = time.time()

while time.time() - start < 3600:
    # Random load between 20-50A with small variations
    a = 35 + random.uniform(-5, 5)
    b = 32 + random.uniform(-5, 5)
    c = 38 + random.uniform(-5, 5)
    
    client.publish("/esp32CTSimulator/PhaseA_Amp", f"{a:.1f}")
    client.publish("/esp32CTSimulator/PhaseB_Amp", f"{b:.1f}")
    client.publish("/esp32CTSimulator/PhaseC_Amp", f"{c:.1f}")
    
    elapsed = int(time.time() - start)
    print(f"[{elapsed}s] A={a:.1f}A, B={b:.1f}A, C={c:.1f}A")
    
    time.sleep(5)  # Update every 5s

print("✓ Test complete!")
EOF

python3 test_longterm.py
```

### Jede 10 Minuten überprüfen

```
Fragen beantworten:
  ✓ WiFi noch connected?
  ✓ MQTT noch connected?
  ✓ Slave noch connected?
  ✓ DAC/Signal immer noch glatt?
  ✓ Keine Fehler im Serial Monitor?
  ✓ Harvi zeigt immer noch realistische Werte?
```

Nach 1 Stunde:
```
Erwartet:
  • Keine Crashes
  • Keine Memory Leaks
  • Keine Disconnects
  • Harvi immer noch gleichmäßig
```

---

## Troubleshooting Matrix

| Problem | Ursache | Lösung |
|---------|---------|--------|
| Master verbindet nicht zu WiFi | SSID/Passwort falsch | config.h überprüfen |
| MQTT Broker nicht erreichbar | Broker offline | MQTT_SERVER überprüfen |
| Slave zeigt "Master: Offline" | UART Kabel locker | Kabel neu anschließen |
| CRC Error in Logs | Baud-Rate mismatch | Auf 115200 reduzieren |
| DAC Output immer 1.65V | Zu wenig Power | Stärker Stromversorgung |
| Oszi zeigt Rausch | EMI in der Umgebung | Shielded Kabel, Filter |
| Harvi zeigt falsche Werte | Signal nicht richtig | Oszi-Messung durchführen |

---

## Test Checkliste zum Abhaken

- [ ] **Test 1**: Standalone Single Board funktioniert
- [ ] **Test 2**: Master-Slave UART Verbindung stabil
- [ ] **Test 3**: Oszi-Messungen alle im Spec
- [ ] **Test 4**: Signal-Integrität über alle Pegel
- [ ] **Test 5**: MQTT Re-Connect funktioniert
- [ ] **Test 6**: Power Consumption in Range
- [ ] **Test 7**: Harvi/Zappi integration erfolgreich
- [ ] **Test 8**: Langzeit-Stabilität bestanden

---

**Geschätzter Gesamttest-Zeiten: 90-120 Minuten**

Wenn alle Tests ✓, bist du **production-ready**! 🎉
