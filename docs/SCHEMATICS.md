# Schaltpläne & Messanleitungen

## 1. Master-Slave UART Verbindung

### Schaltplan: UART Signalleitung

```
┌─────────────────────────────────────────────────────────┐
│                    Master ESP32                         │
├─────────────────────────────────────────────────────────┤
│                    UART Connection                      │
│                                                          │
│   GPIO17 (UART2_TX) ───────┬──────────────────         │
│                            │                  │         │
│                         [1kΩ]  (optional)     │         │
│                            │                  │         │
│                            └──────────────────┼────────►│
│                                               │  RX    │
│                                               │        │
│   GPIO16 (UART2_RX) ◄──────────────────────┼────────┐│
│                                            │        ││
│                         [1kΩ]              │        │
│       (optional for protection)           │        │
│                                            └────────┤
│                                                  TX   │
│                                                      │
│   GND ──────────────────────────────────────────────┼─ GND
│                                                      │
└──────────────┬──────────────────────────────────────┘
               │
        [Shielded Cable]  
       460800 baud, 8N1
               │
        ┌──────┴──────────────────────────────────────┐
        │              Slave ESP32                    │
        ├─────────────────────────────────────────────┤
        │                                              │
        │   GPIO16 (UART2_RX) ◄── RX                  │
        │   GPIO17 (UART2_TX) ──► TX                  │
        │   GND ──────────────────── GND              │
        │                                              │
        └─────────────────────────────────────────────┘


Legend:
TX/RX should use:
  • Cat5e/Cat6 shielded twisted pair (preferred)
  • OR 3 individual shielded wires  
  • Keep cable < 3m for 460800 baud
  • 1kΩ resistors optional (protect GPIO pins)
```

---

## 2. Signal-Ausgabe Schaltplan (Standalone)

### Alle 3 Phasen auf einem Board

```
┌─────────────────────────────────────────────────────┐
│            ESP32 Standalone                         │
├─────────────────────────────────────────────────────┤
│                                                     │
│  GPIO25 (DAC1) ─────[10kΩ]──┬──► Phase A Signal  │
│                             │                      │
│                         [0.1µF]⏚ (Optional Filter)
│                             │                      │
│                            GND                     │
│                                                     │
│  GPIO26 (DAC2) ─────[10kΩ]──┬──► Phase B Signal  │
│                             │                      │
│                         [0.1µF]⏚                  │
│                             │                      │
│                            GND                     │
│                                                     │
│  GPIO32 (PWM)  ─────[1kΩ]───┬──► Phase C Signal  │
│  OR GPIO33         (if PWM)  │                     │
│                       ├─[1kΩ]┤ RC Filter          │
│                       └─[1µF]┤ if PWM enabled     │
│                             │                      │
│                            GND                     │
│                                                     │
│  GND ──────────────────────────────────────────────┤
│         (Common Ground with Harvi)                 │
│                                                     │
└─────────────────────────────────────────────────────┘
         │ USB Power (5V, 500mA)
         └──► or Regulated 5V Supply


To Harvi/Zappi:
┌──────────────────────────────────────────┐
│  Phase A Signal ◄──────────────────────  │
│  Phase B Signal ◄──────────────────────  │
│  Phase C Signal ◄──────────────────────  │
│  GND (Common) ◄────────────────────────  │
└──────────────────────────────────────────┘
```

---

## 3. Signal-Ausgabe Schaltplan (Master)

### Master: Nur Phase A

```
┌─────────────────────────────────────────────────────┐
│            Master ESP32                             │
├─────────────────────────────────────────────────────┤
│                                                     │
│  GPIO25 (DAC1) ─────[10kΩ]──┬──► Phase A Signal  │
│  (to Harvi)                  │                      │
│                          [0.1µF]⏚                  │
│                              │                      │
│                             GND                     │
│                                                     │
│  GPIO26 (DAC2) ────X (unused in master mode)      │
│                                                     │
│  GPIO17 (TX) ──┐                                    │
│                │                                    │
│  GPIO16 (RX) ──┼──► UART to Slave (see section 1) │
│                │                                    │
│  GND ──────────┘                                    │
│                                                     │
│  [WiFi Antenna] ◄─── for MQTT connectivity        │
│                                                     │
└─────────────────────────────────────────────────────┘
         │ USB Power (5V, 500mA)
         └──► for WiFi + MQTT + Master processor


To Harvi/Zappi:
┌──────────────────────────┐
│  Phase A Signal ◄────────│
│  GND (Common) ◄─────────│
│                          │
│ (Phase B, C from Slave)  │
└──────────────────────────┘
```

---

## 4. Signal-Ausgabe Schaltplan (Slave)

### Slave: Phase B + C

```
┌─────────────────────────────────────────────────────┐
│            Slave ESP32                              │
├─────────────────────────────────────────────────────┤
│                                                     │
│  GPIO25 (DAC1) ─────[10kΩ]──┬──► Phase B Signal  │
│  (to Harvi)                  │                      │
│                          [0.1µF]⏚                  │
│                              │                      │
│                             GND                     │
│                                                     │
│  GPIO26 (DAC2) ─────[10kΩ]──┬──► Phase C Signal  │
│  (to Harvi)                  │                      │
│                          [0.1µF]⏚                  │
│                              │                      │
│                             GND                     │
│                                                     │
│  GPIO16 (RX) ──┐                                    │
│                ├──► UART from Master               │
│  GPIO17 (TX) ──┤     (see section 1)               │
│                │                                    │
│  GND ──────────┘                                    │
│                                                     │
│ ✓ WiFi OFF (low power)                             │
│ ✓ Bluetooth OFF                                    │
│                                                     │
└─────────────────────────────────────────────────────┘
         │ USB Power (5V, 300mA) or Battery
         └──► Minimal power consumption (~40mA)


To Harvi/Zappi:
┌──────────────────────────┐
│  Phase B Signal ◄────────│
│  Phase C Signal ◄────────│
│  GND (Common) ◄─────────│
│                          │
│ (Phase A from Master)    │
└──────────────────────────┘
```

---

## 5. Burden Resistor für CT-Sensor (Optional aber Empfohlen)

Wenn du CT-Sensoren echte Last zuführen willst:

```
ESP32 DAC Output
     │
     ├──[10kΩ]────┬──────► Protection Resistor (to GPIO)
     │            │
     ▼       [16Ω, 0.25W]   Burden Resistor
                  │          (YHDC SCT-013 standard)
     ┌────────────┼────────────┐
     │            │            │
    [CT Sensor]   │       [0.1µF]   Noise Filter
   ///////        │            │
    (P)  (S)      │           ⏚
    │    │        │            
    │    └────┼───┴────────┐
    │         │            │
    │    CT Output Signal   │
    │    (to Harvi ADC or   │
    │     Schmitt Trigger)   │
    │                        │
    └────────────────────────────► GND
                            

Component Values:
• Burden: 16Ω, 0.25W (typical for YHDC SCT-013-000)
• Protection: 10kΩ (limits GPIO current)
• Filter: 0.1µF ceramic (high frequency noise)
• CT Ratio: 1000:1 (standard)
```

---

## 6. Oszilloskop-Messschaltung

### Minimale Test-Setup

```
┌──────────────────────────────────────┐
│       Oszilloskop                    │
│                                      │
│  CH1 (Probe 1) ─────────┐            │
│  CH2 (Probe 2) ────┐    │            │
│  CH3 (Probe 3) ──┐ │    │            │
│  GND ────────────┼─┼────┼────────┐   │
└──────────────────┼─┼────┼────────┘
                   │ │    │
                   │ │    ▼
         ┌─────────┼─┼──────────────┐
         │         │ │              │
    GPIO25 DAC ────┘ │          ESP32
    GPIO26 DAC ──────┤          GND
    GND ─────────────┘


Setup:
1. Probe 1: CH1 auf GPIO25 (Phase A)
2. Probe 2: CH2 auf GPIO26 (Phase B)  
3. Probe 3: CH3 auf GPIO32 (Phase C, falls PWM)
4. GND: Alle GND gemeinsam


Measurement Tips:
• Set Oszi to ~100V/div input (DAC output nur 3.3V!)
• Time base: 5ms/div (für 50Hz = 20ms cycle)
• Trigger: Rising edge on CH1
• Averaging: 4-16 samples für noise reduction
```

---

## 7. MQTT Publisher Test-Schaltung

```
┌─────────────────────────────────┐
│     Computer/Laptop             │
├─────────────────────────────────┤
│                                 │
│  mosquitto_pub / Python Script  │
│           │                     │
│           ▼                     │
│    MQTT Topic Publisher         │
│                                 │
└─────────────────────────────────┘
        │ WiFi/Ethernet
        │
        ▼
┌─────────────────────────────────┐
│     MQTT Broker                 │
│   (Mosquitto / Home Assistant)  │
├─────────────────────────────────┤
│  • IP: 192.168.1.100            │
│  • Port: 1883                   │
│  • Topics:                      │
│    home/power/phase_a/current   │
│    home/power/phase_b/current   │
│    home/power/phase_c/current   │
└─────────────────────────────────┘
        │ WiFi
        │
        ▼
┌─────────────────────────────────┐
│     Master ESP32                │
├─────────────────────────────────┤
│  • Receives MQTT messages       │
│  • Generates Phase A DAC        │
│  • Sends Phase B,C to Slave     │
└─────────────────────────────────┘
        │ UART 460800 baud
        │
        ▼
┌─────────────────────────────────┐
│     Slave ESP32                 │
├─────────────────────────────────┤
│  • Receives UART packets        │
│  • Generates Phase B,C DAC      │
│  • No WiFi = Low Power          │
└─────────────────────────────────┘
        │ DAC Outputs
        │
        ▼
┌─────────────────────────────────┐
│     Harvi / Zappi               │
├─────────────────────────────────┤
│  • Reads Phase A, B, C signals  │
│  • Calculates current values    │
│  • Displays on display          │
└─────────────────────────────────┘
```

---

## 8. Verkabelungs-Checkliste

```
Master ESP32:
  ☐ USB Power (5V) connected
  ☐ TX (GPIO17) → Slave RX (GPIO16)
  ☐ RX (GPIO16) ← Slave TX (GPIO17)
  ☐ GND common with Slave
  ☐ GPIO25 DAC → Oszi CH1 (optional test)
  ☐ GPIO25 DAC → Harvi Phase A input
  ☐ GND → Harvi GND

Slave ESP32:
  ☐ USB Power (5V) connected
  ☐ RX (GPIO16) ← Master TX (GPIO17)
  ☐ TX (GPIO17) → Master RX (GPIO16)
  ☐ GND common with Master
  ☐ GPIO25 DAC → Oszi CH2 (optional test)
  ☐ GPIO26 DAC → Oszi CH3 (optional test)
  ☐ GPIO25 DAC → Harvi Phase B input
  ☐ GPIO26 DAC → Harvi Phase C input
  ☐ GND → Harvi GND

Oszilloskop (wenn Messung):
  ☐ CH1 Probe → Master GPIO25 (Phase A)
  ☐ CH2 Probe → Slave GPIO25 (Phase B)
  ☐ CH3 Probe → Slave GPIO26 (Phase C)
  ☐ GND Probe → Common GND all boards
  ☐ GND return to all boards
```

---

## 9. Stromversorgung Optionen

### Option A: USB Dual Power (Laborbench)
```
Computer
   │ USB1
   ▼
Master ESP32 ──────── (Power + Debug Serial)
   
Laptop/PC
   │ USB2
   ▼
Slave ESP32 ────────── (Power + Debug Serial)

✓ Einfach zum Debuggen
✓ Separate Stromversorgung
✓ Serielle Debug für beide
✗ 2 USB-Ports erforderlich
✗ Nicht für Deployment
```

### Option B: Gemeinsame USB Power (Standalone Test)
```
USB Power Supply (5V/2A)
   ├──► Master ESP32
   └──► Slave ESP32
        (gemeinsame GND)

✓ Einfach, nur 1 Stromquelle
✓ Für Feld-Deployment geeignet
✗ Müssen Serielle per separat debuggen
✗ Power Budget teilen
```

### Option C: Dedicated Power für Slave (Low Power)
```
Laptop/USB ─► Master ESP32

5V/1A Battery  
   │ USB Connector
   └──► Slave ESP32 (optional boost converter)

✓ Slave ultra-niedrig Stromverbrauch
✓ Lange Batterielebensdauer
✗ Extra Hardware
✗ Komplexeres Setup
```

---

## 10. Signal-Verifikation mit Multimeter

Wenn du kein Oszi hast:

```
Multimeter DC Mode:

1. Resting Voltage (0A):
   GPIO25, GPIO26 → ~1.65V (Sollwert: 1.6-1.7V)
   
2. With 25A Signal:
   GPIO25 → ~2.0V (Peak positive swing)
   or ~1.3V (Peak negative swing)
   
   Expected range: 1.65V ± 0.8V
   (1.65V DC bias ± 0.8V AC swing)

3. Frequency (benötigt True RMS DMM):
   Frequency meter: 50Hz (± 0.1Hz)
   
4. Debug:
   If voltage stuck at 0V  → Check GPIO pin
   If voltage stuck at 3.3V → Check DAC enable
   If voltage <1.6V → Check software calibration
```

---

## 11. Debugging Tipps

### Signal zu schwach/zu stark

```
DAC Output: 0-255 (8-bit) → Maps:
  0   = 0.0V
  128 = 1.65V (DC Bias / 0A zero-point)
  255 = 3.3V

Current to DAC mapping:
  0A   → DAC 128
  50A  → DAC 128 ± 64
  100A → DAC 128 ± 128


Messung:
• 4A Signal:  DAC ≈ 128 + (4/100)*128 = 133
• 25A Signal: DAC ≈ 128 + (25/100)*128 = 160
• 50A Signal: DAC ≈ 128 + (50/100)*128 = 192
• 100A Signal: DAC ≈ 128 ± 128 = 0 to 255
```

---

Diese Schaltpläne sollten dir alles zeigen, was du brauchst! Möchtest du jetzt die **Test-Anleitung** und **Einkaufsliste**?
