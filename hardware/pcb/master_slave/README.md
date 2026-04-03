# Master-Slave PCB (Carrier Board)

This folder contains a practical PCB starter package for the Master-Slave variant:
- One board hosts two ESP32 DevKit modules (Master + Slave)
- UART between modules is routed on-board
- Phase A/B/C analog outputs are exposed via one terminal connector

## Files
- netlist.csv: Logical net connections
- bom.csv: PCB BOM (components and footprints)
- placement.csv: Suggested XY placement baseline
- fab_notes.md: Manufacturing and routing constraints

## Board concept
- Left socket: Master ESP32 (WiFi + MQTT + Phase A)
- Right socket: Slave ESP32 (Phase B + Phase C)
- Shared 5V input terminal and common GND
- Output terminal to Harvi/Zappi with 4 pins: A/B/C/GND

## Recommended KiCad workflow
1. Create new KiCad project in this folder
2. Place footprints from bom.csv
3. Use placement.csv XY hints
4. Route nets from netlist.csv
5. Apply routing constraints from fab_notes.md
6. Run ERC/DRC
7. Export Gerber + Drill + BOM + CPL

## Expected firmware pin mapping
- Master GPIO25 -> Phase A
- Slave GPIO25 -> Phase B
- Slave GPIO26 -> Phase C
- Master GPIO17 -> Slave GPIO16
- Slave GPIO17 -> Master GPIO16

## Notes
- This is a carrier-board style design for ESP32 DevKit modules
- No mains wiring on PCB
- Low-voltage signals only
