# Einkaufsliste – Komplettes Material

## 📋 Übersicht

**Minimum Setup** (Standalone, 1 Board): ~€50
**Empfohlen** (Master-Slave, 2 Boards): ~€85
**Vollständiges Set** (mit Test-Equipment): ~€300+

---

## 1️⃣ KERNKOMPONENTEN

### Mikrocontroller

| Item | Modell | Menge | Preis/€ | Total | Link |
|------|--------|-------|---------|-------|------|
| **ESP32 Development Board** | ESP32 DOIT DevKit v1 | 1x (Standalone) oder 2x (Master-Slave) | 8-12 | 12-24 | [AliExpress](https://de.aliexpress.com/item/4000242274906.html) |
| | | | | | [Amazon.de](https://www.amazon.de/AZDelivery-NodeMCU-Development-Bluetooth-Compatible/dp/B074RGW2VQ) |

**Empfehlung:** 2x kaufen (nur €2-3 mehr) = Backup und für Tests

---

## 2️⃣ STROMVERSORGUNG & USB

### USB Kabel & Adapter

| Item | Spezifikation | Menge | Preis/€ | Total | Link |
|------|---------------|-------|---------|-------|------|
| **USB Micro-B Kabel** | Standard, 2m | 2-3x | 2-5 | 4-15 | [Amazon.de](https://www.amazon.de/AmazonBasics-Micro-USB-Schnellladekabel/dp/B00MIH6HYE) |
| **USB Power Adapter** | 5V/2A (zum Aufladen) | 1-2x | 5-15 | 5-30 | [Amazon.de](https://www.amazon.de/Anker-PowerPort-Adapter-Devices-Compatible/dp/B00VH8ZW02) |
| **USB Hub** | 4+ Ports, powered | 1x (optional) | 15-25 | 15-25 | [Amazon.de](https://www.amazon.de/Anker-Port-Hub-Ultra-Slim/dp/B00MYYWPHZ) |

**Hinweis:** Mit 2-3 USB Kabeln kannst du parallel programmieren & monitoren

---

## 3️⃣ VERBINDUNGSKABEL & DRÄHTE

### UART Kommunikation (Master ↔ Slave)

| Item | Spezifikation | Menge | Preis/€ | Total | Link |
|------|---------------|-------|---------|-------|------|
| **Shielded Twisted Pair Kabel** | Cat5e/Cat6, grau, 5m | 1x | 5-10 | 5-10 | [Amazon.de](https://www.amazon.de/Wentronic-Netzwerk-Kabel-Cat6-5m/dp/B00IYGDVTQ) |
| **Alternativ: 3x Shielded Wire** | Einzelne 22AWG, 5m | 1x Set | 3-8 | 3-8 | [AliExpress](https://de.aliexpress.com/item/32993922970.html) |
| | | | | | |
| **Jumper Cable Set** | Dupont-Stecker, M/M+F/M | 1x | 5-10 | 5-10 | [Amazon.de](https://www.amazon.de/AZDelivery-Jumper-Drucker-Raspberry-Arduino/dp/B072BXN2CX) |

**Für Master-Slave:** Shielded Cable (bessere Qualität & Störuntersuchung)
**Für Test-Bench:** Jumper Cables (billig & flexibel)

---

## 4️⃣ PASSIVE KOMPONENTEN (Elektronik)

### Widerstände

| Item | Wert | Toleranz | Leistung | Menge | Preis/€ | Total | Link |
|------|------|----------|----------|-------|---------|-------|------|
| **Widerstand** | 10kΩ | 5% | 0.25W | 10x | 0.50 | 5 | [Amazon.de](https://www.amazon.de/AmazonBasics-Widerstände-Sortiment-Kohleschicht-Toleranzen/dp/B07PKHF82G) |
| **Widerstand** | 1kΩ | 5% | 0.25W | 10x | 0.50 | 5 | (Set beides) |
| **Widerstand** | 16Ω | 5% | 0.25W | 5x | 1.00 | 5 | [AliExpress](https://de.aliexpress.com/item/32829615447.html) |

### Kondensatoren (Filter)

| Item | Kapazität | Typ | Spannung | Menge | Preis/€ | Total | Link |
|------|-----------|-----|----------|-------|---------|-------|------|
| **Kondensator** | 0.1µF | Keramik | 16V+ | 20x | 0.30 | 6 | [Amazon.de](https://www.amazon.de/BOJACK-mikrofarad-Multipack-Kondensator-Hohe/dp/B07KYWRQ4D) |
| **Kondensator** | 1µF | Keramik | 16V+ | 10x | 0.50 | 5 | (Set) |
| **Kondensator** | 10µF | elektrolytisch | 16V+ | 5x | 1.00 | 5 | (Set) |

**Gesamtkosten Passive:** ~€30 (aber Set ist günstiger)

---

## 5️⃣ VERBINDUNGSTECHNIK

### Stecker & Adapter

| Item | Typ | Menge | Preis/€ | Total | Link |
|------|-----|-------|---------|-------|------|
| **Alligator Kabel** | Rot/Schwarz, 2m | 2x | 3-5 | 6-10 | [Amazon.de](https://www.amazon.de/Alligator-Verbindungskabel-Testleitung-Prüfleitung-Sprungdrähte/dp/B081KWHZH6) |
| **Steckboard (Breadboard)** | 170 Kontakte | 2x | 2-3 | 4-6 | [Amazon.de](https://www.amazon.de/Elegoo-170-Loch-Steckbrett-Jumper-Drähte/dp/B00WXYD4R0) |
| **Breadboard Stecker Set** | 12x Breakaway Stecker | 1x | 3-5 | 3-5 | [AliExpress](https://de.aliexpress.com/item/32855239159.html) |

---

## 5️⃣b PCB-TEILE (MASTER-SLAVE CARRIER BOARD)

| Item | Spezifikation | Menge | Preis/€ | Total | Link |
|------|---------------|-------|---------|-------|------|
| **PCB Fertigung** | 2-layer FR4, 120x60mm, 1oz, 1.6mm | 5x | 10-25 | 10-25 | [JLCPCB](https://jlcpcb.com) |
| **ESP32 Header Buchsen** | 2.54mm, 1x19 female, long-pin | 4x | 1-3 | 4-12 | [Amazon.de](https://www.amazon.de/dp/B07K9ZQX1N) |
| **Schraubklemme 2-polig** | 5.08mm pitch | 1x | 0.5-2 | 0.5-2 | [AliExpress](https://de.aliexpress.com/item/1005002667861539.html) |
| **Schraubklemme 4-polig** | 5.08mm pitch | 1x | 1-3 | 1-3 | [AliExpress](https://de.aliexpress.com/item/1005002667861539.html) |
| **Polyfuse** | 1A hold, 1206 | 1x | 0.3-1 | 0.3-1 | [Mouser](https://www.mouser.de) |
| **Schottky Diode** | SS14 (SMA) | 1x | 0.1-0.5 | 0.1-0.5 | [Mouser](https://www.mouser.de) |
| **Mounting Hardware** | M3 spacer + screws set | 1x | 3-8 | 3-8 | [Amazon.de](https://www.amazon.de/dp/B07D6M8G2Q) |

PCB-Designdateien im Repo:
- [docs/PCB_MASTER_SLAVE.md](docs/PCB_MASTER_SLAVE.md)
- [hardware/pcb/master_slave/README.md](hardware/pcb/master_slave/README.md)

---

## 5️⃣c PCB-TEILE (MASTER-SLAVE 230V EINGANG)

| Item | Spezifikation | Menge | Preis/€ | Total | Link |
|------|---------------|-------|---------|-------|------|
| **AC/DC Modul** | HLK-PM01 (5V/3W) oder IRM-05-5 | 1x | 4-10 | 4-10 | [Mouser](https://www.mouser.de) |
| **Primarsicherung + Halter** | 5x20mm, trage 250mA | 1x | 2-6 | 2-6 | [Reichelt](https://www.reichelt.de) |
| **MOV** | 275VAC, 14D471K | 1x | 0.5-2 | 0.5-2 | [Mouser](https://www.mouser.de) |
| **NTC** | 10D-9 (Inrush Limiter) | 1x | 0.5-2 | 0.5-2 | [Mouser](https://www.mouser.de) |
| **PCB Fertigung HV/LV** | 2-layer, Frasschlitz, 126x66mm | 5x | 15-35 | 15-35 | [JLCPCB](https://jlcpcb.com) |

Designdateien fur diese Variante:
- [docs/PCB_MASTER_SLAVE_230V.md](docs/PCB_MASTER_SLAVE_230V.md)
- [hardware/pcb/master_slave_230v/README.md](hardware/pcb/master_slave_230v/README.md)

---

## 🔧 MESS-INSTRUMENTE (Für Tests & Debugging)

### USB Power Meter (Optional aber Empfohlen)

| Item | Funktion | Preis/€ | Link |
|------|----------|---------|------|
| **USB Power Meter** | Misst Amp, Volt, Watt | 15-25 | [Amazon.de](https://www.amazon.de/RUIDENG-Zyklops-USB-Strom-Multi-Tester-Voltmeter/dp/B01H82Z7MM) |

**Nutzen:** Kann PowerConsumption messen, Debugging bei Stromverbrauch-Problemen

---

### Digitales Multimeter (Empfohlen)

| Modell | Specs | Preis/€ | Link |
|--------|-------|---------|------|
| **UNI-T UT61D** | True RMS, AC/DC, 20-1000V | 30-45 | [Amazon.de](https://www.amazon.de/UNI-T-UT61D-Digital-Multimeter-Datenerfassung/dp/B092L7P7ZJ) |
| **Victory VC-830L** | Basic, AC/DC, 20-700V | 20-30 | [Amazon.de](https://www.amazon.de/Voltcraft-VC-830L-Digitales-Multimeter-Wechselstrom/dp/B00LWW1G5A) |

**Nutzen:** DC Offset überprüfen (sollte ~1.65V sein)

---

### Digitales Oszilloskop (Optional, für echte Qualitäts-Messungen)

| Modell | Kanäle | Bandbreite | Preis/€ | Link |
|--------|--------|-----------|---------|------|
| **Rigol DS1054Z** | 4 | 50MHz | 280-350 | [Amazon.de](https://www.amazon.de/Rigol-DS1054Z-Digital-Oscilloscope-Channels/dp/B00DP7K2M2) |
| | | | | [Banggood](https://www.banggood.com/100M-Bandwidth-4-Channel-Digital-Oscilloscope-Rigol-DS1054Z-p-958269.html) |
| **Siglent SDS1104X-E** | 4 | 100MHz | 350-400 | [AliExpress](https://de.aliexpress.com/item/32949413971.html) |

**Hinweis:** Luxus, aber wenn du magst, ist die Rigol ausgezeichnet. Für Hobby: Skip.

---

## 🎯 OPTIONALE ADD-ONS

### Für Production Deployment

| Item | Zweck | Menge | Preis/€ | Total |
|------|-------|-------|---------|-------|
| **DIN-Rail Bügel** | Zur Befestigung | 1x | 3-5 | 3-5 |
| **Enclosure/ Gehäuse** | Schutz | 1x | 15-30 | 15-30 |
| **Thermische Pads** | Wärmeleitung | 1x Set | 5-8 | 5-8 |

### Für Extended Range (100A+)

| Item | Zweck | Preis/€ |
|------|-------|---------|
| **External DAC (MCP4822)** | Höhere Auflösung | 3-5 |
| **OpAmp Buffer (LM358)** | Signal Verstärkung | 1-2 |
| **Power Supply 5V/3A** | Stärkere Stromversorgung | 10-20 |

---

## 📦 SETS ZUM KAUFEN (Empfohlen)

### Set A: Standalone Minimal (€40-60)

```
✓ 1x ESP32 DOIT DevKit        €12
✓ 1x USB Kabel + Power         €5
✓ 1x Jumper Cables Set         €5
✓ 1x Widerstand/Kondensator Set €10
✓ 1x Breadboard                €3
✓ 1x Alligator Kabel Set       €8
───────────────────────────
Total: €43 (vor Rabatten)
```

**Nutzen:** Schnelle Prototyping, testen ob System funktioniert

---

### Set B: Master-Slave Recommended (€80-100)

```
✓ 2x ESP32 DOIT DevKit         €24
✓ 2x USB Kabel + Power          €10
✓ 1x Shielded Cat5e Kabel      €8
✓ 1x Jumper Cables Set         €5
✓ 1x Widerstand/Kondensator Set €10
✓ 2x Breadboards               €6
✓ 1x Alligator Kabel           €8
✓ 1x Digitales Multimeter      €25
───────────────────────────
Total: €96 (vor Rabatten)
```

**Nutzen:** Production-ready, mit Mess-Equipment für Debugging

### Set B+ : Master-Slave mit eigener Platine (€110-140)

```
✓ Alles aus Set B                €96
✓ PCB Fertigung (5 Stuck)        €20
✓ PCB Header + Klemmen            €12
✓ M3 Abstandshalter/Schrauben      €6
───────────────────────────
Total: €134 (typisch)
```

**Nutzen:** Sauberer Aufbau, weniger Verkabelungsfehler, wartungsfreundlich

---

### Set C: Full Professional (€280-350)

```
✓ Set B (Master-Slave)         €96
✓ 1x Rigol DS1054Z Oszi        €320
✓ 1x USB Power Meter           €20
✓ Diverse Ersatz-Komponenten   €20
───────────────────────────
Total: €456 (Full Lab)
```

**Nutzen:** Professionelle Entwicklung & Troubleshooting

### Set D: Master-Slave mit 230V Eingang (€130-180)

```
✓ Alles aus Set B                €96
✓ 230V Schutz- und Netzteilteile  €15
✓ HV/LV PCB Fertigung             €25
✓ Reserve/Qualitatsaufschlag      €20
───────────────────────────
Total: €156 (typisch)
```

**Nutzen:** Kompakte Einplatinen-Losung ohne externes 5V-Netzteil
**Achtung:** Nur fur qualifizierte HV-Entwicklung und sichere Einhausung

---

## 🛒 SAMMELBESTELLUNG (Billig & Schnell)

### Einkaufsquellen nach Region

**Deutschland/EU:**

```
AmazonDE:
  • ESP32: €12
  • USB: €5
  • Kabel: €10
  → Schnelle Lieferung (1-2 Tage Prime)

CulturaWIDGETS.de:
  • Elektronik: €8-15
  → Spezialisiert auf Maker

ELV.de:
  • Professionelle Komponenten
  → Etwas teurer
```

**International (günstiger):**

```
AliExpress:
  • 50% günstiger als Amazon
  • ESP32: €6-8
  • Komponenten Bundles
  → 2-4 Wochen Liefering

Banggood:
  • ähnlich wie AliExpress
  → Express Versand möglich
```

---

## 🔗 SHOPPING CHECKLISTE

### Minimum (Test-Betrieb)

- [ ] 1x ESP32 DOIT DevKit (€12)
- [ ] 1x USB Kabel + Power Adapter (€5)
- [ ] 1x Jumper Cable Set (€5)
- [ ] 1x Passive Component Kit (€10)
- [ ] 1x Breadboard (€3)

**Subtotal: €35**

---

### Recommended (Production)

- [ ] 2x ESP32 DOIT DevKit (€24)
- [ ] 2x USB Kabel + Power (€10)
- [ ] 1x Shielded UART Kabel (€8)
- [ ] 1x Passive Component Kit (€10)
- [ ] 2x Breadboards (€6)
- [ ] 1x Digitales Multimeter (€25)
- [ ] 1x Alligator Cables (€8)

**Subtotal: €91**

---

### Professional (Lab + Debug)

- [ ] All Recommended Items (€91)
- [ ] 1x Digitales Oszi (€300+)
- [ ] 1x USB Power Meter (€20)
- [ ] 1x Enclosure/Gehäuse (€20)
- [ ] Ersatz-komponenten (€20)

**Subtotal: €460+**

---

## 💡 GELD SPAREN TIPPS

1. **Bulk kaufen:** Jede Komponente ist günstiger in 10er-Sets
2. **Ebay Kleinanzeigen:** Gebrauchte Oszi häufig 50% günstiger
3. **Uni/Schule:** Oft Elektronik-Material kostenlos!
4. **AliExpress Coupons:** 5-10% Rabatt auf größere Bestellungen
5. **Mit Freunden teilen:** 2x ESP32 kaufen, teilen = €6/person

---

## 📋 BESTELLFORMULAR (DIY)

Kopiere dies in deine Notizen:

```
┌─ BESTELLUNG ──────────────────────────────────────┐
│ Ziel: [ ] Standalone [ ] Master-Slave [ ] Full   │
│                                                    │
│ Zeit: ______ (Wann brauchst du es?)               │
│ Budget: ______€                                    │
│                                                    │
│ MUSS-HABEN:                                       │
│ ☐ ESP32 Qty: ___                                  │
│ ☐ USB Kabel Qty: ___                              │
│ ☐ Jumper Cables                                   │
│ ☐ Passive Components                              │
│                                                    │
│ OPTIONAL:                                          │
│ ☐ Multimeter                                       │
│ ☐ Oszi                                             │
│ ☐ Power Meter                                      │
│                                                    │
│ QUELLE:                                            │
│ ☐ Amazon [ ] AliExpress [ ] Local                 │
│                                                    │
│ NOTIZEN:                                           │
│ ────────────────────────────────────────────────   │
│                                                    │
└────────────────────────────────────────────────────┘
```

---

## 🚚 VERSAND & LIEFERN

| Quelle | Liefering | Kosten | Zollagen |
|--------|-----------|--------|----------|
| Amazon Prime | 1-2 Tage | Kostenlos | Nein |
| AliExpress Standard | 14-28 Tage | Kostenlos | Nein (<€150) |
| AliExpress Express | 5-7 Tage | €3-8 | Nein |
| Banggood DHL | 7-10 Tage | €10 | Ja (>€150) |

---

## 💰 BUDGET-ZUSAMMENFASSUNG

| Szenario | Material | Shipping | Gesamt |
|----------|----------|----------|--------|
| **Minimal** | €35 | Kostenlos | **€35** |
| **Recommended** | €91 | Kostenlos | **€91** |
| **Professional** | €460 | Versand | **€480+** |

---

**Geschätzter Gesamt-Setup-Zeit:**
- Bestellung: 5 min
- Lieferung: 1-28 Tage (abhängig von Quelle)
- Zusammenbau: 15-30 min
- **Testen: 2 Stunden**

**Bereit? → Siehe [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md)** 🚀
