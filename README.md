# Myenergi CT Clamp Simulator

ESP32-based simulator for Myenergi CT clamp inputs with support for standalone, master and slave operation.

## Deutsch

Kurze Projektzusammenfassung:

- Simuliert 3-phasige CT-Signale fur Harvi/Zappi-ahnliche Eingange
- MQTT-Eingang fur `PhaseA_Amp`, `PhaseB_Amp`, `PhaseC_Amp` und `SumPower_kW`
- Webserver auf dem Master mit WiFi- und MQTT-Konfiguration, Live-Graph und OTA
- Optionales OLED-Display fur kompakte Laufzeitanzeige
- Master-Slave-Betrieb uber UART fur Phase B und C

## Wichtige MQTT-Topics

Standardpfad:

```text
/esp32CTSimulator/PhaseA_Amp
/esp32CTSimulator/PhaseB_Amp
/esp32CTSimulator/PhaseC_Amp
/esp32CTSimulator/SumPower_kW
/esp32CTSimulator/Status
```

## Build

```bash
cd myenergi_CT-Clamp-Simulator

.venv/bin/pio run -e esp32
.venv/bin/pio run -e esp32-master
.venv/bin/pio run -e esp32-slave
```

## Flashen

```bash
.venv/bin/pio run -e esp32-master -t upload
```

## Hinweis zu den Display-Testdateien in `src`

- `src/display_test_main.cpp` und `src/display_test_u8g2_main.cpp` sind optionale Diagnoseprogramme fur OLED-Tests.
- Sie werden von der normalen Firmware nicht mitgebaut.
- Sie werden nur uber die separaten PlatformIO-Umgebungen `esp32-display-test` und `esp32-display-test-u8g2` verwendet.
- Wenn du keine separaten OLED-Hardwaretests mehr brauchst, kann man sie entfernen.
- Solange du eine einfache Display-Diagnose im Repo behalten willst, sind sie sinnvoll.

## Dokumentation

- Details zum aktuellen Release: [docs/README.md](docs/README.md)
- Schnellstart: [docs/QUICKSTART.md](docs/QUICKSTART.md)
- MQTT-Schnittstelle: [docs/MQTT.md](docs/MQTT.md)
- Master-Slave-Setup: [docs/MASTER_SLAVE_QUICKSTART.md](docs/MASTER_SLAVE_QUICKSTART.md)
- Hardware und Verdrahtung: [docs/HARDWARE.md](docs/HARDWARE.md)
- PCB-Unterlagen: [hardware/pcb/master_slave/README.md](hardware/pcb/master_slave/README.md)

## English

Short project summary:

- Simulates 3-phase CT clamp signals for Harvi/Zappi-like inputs
- MQTT input for `PhaseA_Amp`, `PhaseB_Amp`, `PhaseC_Amp`, and `SumPower_kW`
- Master web server with WiFi and MQTT configuration, live graph, and OTA
- Optional OLED display for compact runtime status
- Master-slave operation over UART for Phase B and Phase C

## Important MQTT Topics

Default path:

```text
/esp32CTSimulator/PhaseA_Amp
/esp32CTSimulator/PhaseB_Amp
/esp32CTSimulator/PhaseC_Amp
/esp32CTSimulator/SumPower_kW
/esp32CTSimulator/Status
```

## Build

```bash
cd myenergi_CT-Clamp-Simulator

.venv/bin/pio run -e esp32
.venv/bin/pio run -e esp32-master
.venv/bin/pio run -e esp32-slave
```

## Flash

```bash
.venv/bin/pio run -e esp32-master -t upload
```

## Note About The Display Test Files In `src`

- `src/display_test_main.cpp` and `src/display_test_u8g2_main.cpp` are optional OLED diagnostic programs.
- They are not built as part of the main firmware targets.
- They are only used by the dedicated PlatformIO environments `esp32-display-test` and `esp32-display-test-u8g2`.
- If you no longer need separate OLED hardware diagnostics, they can be removed.
- If you want to keep a simple display validation path in the repo, they are still useful.

## Documentation

- Current release details: [docs/README.md](docs/README.md)
- Quick start: [docs/QUICKSTART.md](docs/QUICKSTART.md)
- MQTT interface: [docs/MQTT.md](docs/MQTT.md)
- Master-slave setup: [docs/MASTER_SLAVE_QUICKSTART.md](docs/MASTER_SLAVE_QUICKSTART.md)
- Hardware and wiring: [docs/HARDWARE.md](docs/HARDWARE.md)
- PCB files: [hardware/pcb/master_slave/README.md](hardware/pcb/master_slave/README.md)