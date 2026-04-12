# Documentation Index

## Quick Navigation

### 🚀 I'm Starting Fresh

**→ [QUICKSTART.md](QUICKSTART.md)** - 5 minutes to working system
- Single ESP32 setup
- Basic configuration
- First MQTT test

### 🏗️ I Want Master-Slave Setup

**→ [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md)** - Master + Slave deployment
- Step-by-step setup for 2 ESP32s
- UART wiring guide
- Verification checklist

**→ [MASTER_SLAVE.md](MASTER_SLAVE.md)** - Detailed Master-Slave reference
- Architecture overview
- Complete protocol specification
- Troubleshooting & scaling
- Power management

### 🔧 I Need Hardware Details

**→ [HARDWARE.md](HARDWARE.md)** - Physical setup & wiring
- Pin definitions
- CT sensor circuits
- Signal verification
- Oscilloscope measurements
- Troubleshooting electrical issues

**→ [SCHEMATICS.md](SCHEMATICS.md)** - Schaltpläne & Messanleitungen
- Master-Slave UART Verbindung
- Signal-Ausgabe Schaltpläne (alle Modi)
- Burden Resistor Konfiguration
- Oszi-Messschaltung
- Stromversorgung Optionen

**→ [PCB_MASTER_SLAVE.md](PCB_MASTER_SLAVE.md)** - Master-Slave Platine
- Carrier-Board Konzept fur zwei ESP32
- Netlist, BOM und Platzierung
- Fertigungsparameter fur PCB-Hersteller
- KiCad-Workflow von Schema bis Gerber

**→ [PCB_MASTER_SLAVE_230V.md](PCB_MASTER_SLAVE_230V.md)** - Master-Slave Platine mit 230V Eingang
- Zweite Variante mit AC/DC-Modul auf Board
- Schutzstufe (Sicherung, MOV, NTC)
- Isolationszonen und Clearance-Hinweise
- Inbetriebnahme-Reihenfolge fur HV/LV

**→ [SHOPPING_LIST.md](SHOPPING_LIST.md)** - Einkaufsliste mit Links
- Komponenten mit Amazon.de, AliExpress Links
- Preise & Alternativen
- 3 vorkonfigurierte Sets
- Budget-Optionen

### 📡 I Need MQTT Help


**→ [MQTT.md](MQTT.md)** - MQTT configuration & publishing
- Topic reference
- Broker setup (Mosquitto, Docker, Home Assistant)
- Publishing current values
- Performance optimization
- Authentication setup

**→ [TESTING.md](TESTING.md)** - Umfassende Test-Anleitung
- 8 verschiedene Test-Szenarien
- Oszi-Messanweisungen
- MQTT Robustheit testen
- Langzeit-Stabilität
- Troubleshooting Matrix

**→ [WOKWI.md](WOKWI.md)** - Browser simulation setup
- ESP32 + OLED I2C wiring in Wokwi
- MQTT topic simulation over public broker
- Fast bring-up without hardware

### ❓ I Have Questions

**→ [FAQ.md](FAQ.md)** - Frequently asked questions
- Comparison: Standalone vs Master-Slave
- PWM vs DAC differences
- Troubleshooting common issues
- Performance tips

### 📖 Full Documentation

**→ [README.md](README.md)** - Complete overview
- All features
- All configurations
- DAC/PWM comparison
- References and links

---

## Choose Your Path

### Scenario: Single ESP32 with Harvi (Beginner)
1. [QUICKSTART.md](QUICKSTART.md) - Get it running in 5 min
2. [HARDWARE.md](HARDWARE.md) - Connect signals to Harvi
3. [MQTT.md](MQTT.md) - Feed it data

### Scenario: Two ESP32s (Master-Slave, Recommended)
1. [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md) - Wire and program
2. [MASTER_SLAVE.md](MASTER_SLAVE.md) - Detailed config & troubleshooting
3. [MQTT.md](MQTT.md) - Publish current data

### Scenario: Debug Signal Quality
1. [HARDWARE.md](HARDWARE.md) - Review wiring
2. [FAQ.md](FAQ.md) - See oscilloscope measurements
3. [README.md](README.md) - DAC/PWM specifications

### Scenario: Optimize Power Usage
1. [MASTER_SLAVE.md](MASTER_SLAVE.md) - Power management section
2. [FAQ.md](FAQ.md) - Performance tips
3. [README.md](README.md) - Power consumption table

---

## File Summary

| File | Purpose | Read Time |
|------|---------|-----------|
| [QUICKSTART.md](QUICKSTART.md) | Get running fast (single ESP32) | 5 min |
| [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md) | Get running with 2 ESP32s | 10 min |
| [HARDWARE.md](HARDWARE.md) | Physical wiring & circuits | 15 min |
| [SCHEMATICS.md](SCHEMATICS.md) | Schaltpläne & Messschaltungen | 15 min |
| [MQTT.md](MQTT.md) | MQTT configuration guide | 10 min |
| [MASTER_SLAVE.md](MASTER_SLAVE.md) | Master-Slave deep dive | 20 min |
| [TESTING.md](TESTING.md) | Test-Anleitung (8 Szenarien) | 90 min |
| [WOKWI.md](WOKWI.md) | Wokwi simulation setup | 10 min |
| [SHOPPING_LIST.md](SHOPPING_LIST.md) | Einkaufsliste mit Links | 5 min |
| [FAQ.md](FAQ.md) | Q&A and troubleshooting | 10 min |
| [README.md](README.md) | Full documentation | 20 min |

---

## What Each ESP32 Mode Does

### Standalone (Single Board)
```
WiFi ←→ MQTT Broker
  ↓
Master Board
  ├─ GPIO25 (DAC) → Phase A
  ├─ GPIO26 (DAC) → Phase B
  └─ GPIO32 (PWM) → Phase C
```
**Use when:** Simple setup, all signals on one board

### Master-Slave (Two Boards)
```
WiFi ←→ MQTT Broker
  ↓
Master Board
  ├─ GPIO25 (DAC) → Phase A
  └─ GPIO17 (UART TX) ────► Slave
                            ↓
                       Slave Board
                            ├─ GPIO25 (DAC) → Phase B
                            └─ GPIO26 (DAC) → Phase C
```
**Use when:** Dedicated signal generation, low power slave, expandable

---

## Start Here Based on Your Experience

### 🟢 Beginner (Never used ESP32 before)
1. Grab [QUICKSTART.md](QUICKSTART.md)
2. Follow steps 1-5
3. Reference [HARDWARE.md](HARDWARE.md) for wiring

### 🟡 Intermediate (Know ESP32, new to this project)
1. Review [README.md](README.md) overview
2. Choose: [QUICKSTART.md](QUICKSTART.md) OR [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md)
3. Check [MQTT.md](MQTT.md) for data feeding

### 🔴 Advanced (Customizing/troubleshooting)
1. See [MASTER_SLAVE.md](MASTER_SLAVE.md) for architecture
2. Check [HARDWARE.md](HARDWARE.md) for signal specs
3. Use [FAQ.md](FAQ.md) for issues

---

## Key Takeaways

- **📝 QUICKSTART** = "Just make it work"
- **🏗️ MASTER_SLAVE** = "How to use 2 boards"  
- **🔧 HARDWARE** = "How to wire & verify signals"
- **📡 MQTT** = "How to feed it data"
- **❓ FAQ** = "My [X] doesn't work"
- **📖 README** = "All the details"

---

## Still Need Help?

1. Check [FAQ.md](FAQ.md) - Most common questions answered
2. Review [HARDWARE.md](HARDWARE.md) - Wiring diagrams often reveal issues
3. See [MASTER_SLAVE.md](MASTER_SLAVE.md) - Troubleshooting section
4. Read [README.md](README.md) - Complete reference

---

**Last updated:** 2026-04-03
**Latest version:** [GitHub Repository](https://github.com/SSPANNI/myenergi_CT-Clamp-Simulator)
