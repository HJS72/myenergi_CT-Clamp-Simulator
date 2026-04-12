# Master-Slave PCB (Carrier Board)

This folder contains a practical PCB starter package for the Master-Slave variant:
- One board hosts two ESP32 DevKitC V4 NodeMCU modules (Master + Slave)
- UART between modules is routed on-board
- Shared USB-C power input feeds both ESP32 modules
- Phase A/B/C analog outputs are exposed via three 2-pin terminal connectors (each phase + GND)
- Optional 1.3 inch OLED I2C display header for live 3-phase current display

## Files
- netlist.csv: Logical net connections
- bom.csv: PCB BOM (components and footprints)
- placement.csv: Suggested XY placement baseline
- fab_notes.md: Manufacturing and routing constraints
- DEVKITC_V4_PINMAP.md: Full 2x19 pin map and project node mapping

## Board concept
- Left socket: Master ESP32 DevKitC V4 NodeMCU (WiFi + MQTT + Phase A)
- Right socket: Slave ESP32 DevKitC V4 NodeMCU (Phase B + Phase C)
- Shared USB-C 5V input and common GND
- Output terminals to Harvi/Zappi:
	- J4A: Phase A / GND
	- J4B: Phase B / GND
	- J4C: Phase C / GND
- OLED connector:
	- J6-1: 3V3
	- J6-2: GND
	- J6-3: SCL (GPIO22)
	- J6-4: SDA (GPIO21)

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

## ESP32 Module Variant
- Target module: ESP32 DevKitC V4 NodeMCU (38-pin)
- Socket style: two rows of 1x19 pins at 2.54mm pitch
- Verify your exact board revision pin labels before final routing

## Header Numbering Used In Netlist
- `J1A/J2A`: left header row, pins 1..19 from USB side to opposite edge
- `J1B/J2B`: right header row, pins 1..19 from USB side to opposite edge
- Key references used:
	- `A-19_5V` for module 5V input
	- `B-1_GND` for module ground reference
	- `A-9_IO25` and `A-10_IO26` for DAC outputs
	- `B-11_IO17` and `B-12_IO16` for UART interconnect
	- `A-2_EN` for optional reset switch

## Notes
- This is a carrier-board style design for ESP32 DevKitC V4 NodeMCU modules
- No mains wiring on PCB
- Low-voltage signals only
- USB-C is used as power sink only (USB2.0 data lines not used)
