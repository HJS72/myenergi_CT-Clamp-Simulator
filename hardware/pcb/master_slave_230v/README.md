# Master-Slave PCB (230VAC Input Variant)

This folder contains a second PCB variant with mains input and isolated 5V generation.

## Purpose
- One board hosts both ESP32 modules (Master + Slave)
- Board is powered from 230VAC using an isolated AC/DC module
- Same phase output mapping (A/B/C/GND) to Harvi/Zappi

## Files
- netlist.csv: Net-level connectivity
- bom.csv: Suggested BOM for 230VAC variant
- placement.csv: Suggested footprint placement
- fab_notes.md: Isolation and fabrication constraints
- master_slave_230v_carrier.kicad_pro: KiCad project file
- master_slave_230v_carrier.kicad_dru: Design rules incl. HV/LV constraints
- master_slave_230v_carrier.kicad_pcb: Board template with outline, connectors and keepout zone
- MANUFACTURER_NOTES.md: Ready-to-send fabrication and assembly notes

## Architecture
- Primary side (HV): AC input, fuse, MOV, NTC, AC/DC module input
- Isolation barrier: creepage/clearance + slot
- Secondary side (LV): 5V rail, ESP32 sockets, UART and analog outputs

## Output mapping
- Master GPIO25 -> Phase A
- Slave GPIO25 -> Phase B
- Slave GPIO26 -> Phase C
- J5 output terminal: A / B / C / GND

## Important
This is a high-voltage design concept and requires qualified design verification before real deployment.

## Current board template status
- Board outline is defined (126mm x 66mm)
- HV/LV keepout barrier is defined (x=60..68mm)
- Main connectors and module sockets are pre-placed
- Silkscreen warnings for HV area are included
- Pre-routing added for critical power path (L_IN -> F1 -> NTC1 -> PS1, N_IN -> PS1)
- Pre-routing added for secondary path (PS1 -> F2 -> 5V_MAIN and GND_ISO distribution)
- Pre-routing added for output resistors to PHASE terminal block
- Signal matrix headers added:
	- J6 MASTER_SIG: GPIO25, TX, RX, GND nets
	- J7 SLAVE_SIG: GPIO25, GPIO26, RX, TX, GND nets
	- J8 MASTER_DEVKIT_BRIDGE: fixed bridge points to Master module
	- J9 SLAVE_DEVKIT_BRIDGE: fixed bridge points to Slave module
- Silkscreen pin legend added next to J8/J9 for direct assembly reference
- Additional silkscreen legends added for connectors:
	- J1: AC input pin order L/N/PE
	- J2: 5V service connector +5V/GND
	- J5: Harvi/Zappi output pin order A/B/C/GND
	- Protection parts labels (F1, MOV1, NTC1, PS1, F2)

## Fixed bridge mapping (ESP32 DevKit V1 30-pin)
Wire these short bridges during assembly:

Master side:
- J8-1 -> Master GPIO25
- J8-2 -> Master GPIO17
- J8-3 -> Master GPIO16
- J8-4 -> Master GND

Slave side:
- J9-1 -> Slave GPIO25
- J9-2 -> Slave GPIO26
- J9-3 -> Slave GPIO16
- J9-4 -> Slave GPIO17
- J9-5 -> Slave GND

Before fabrication:
1. Verify/adjust footprints against your KiCad libraries
2. Finalize bridge routing from J6/J7 to the exact ESP32 socket pads used on your module revision
3. Run full DRC/ERC with project rules
4. Review creepage/clearance with your compliance target

## Gerber Preflight Checklist

Use this checklist before sending to a PCB manufacturer.

### 1. Electrical and rule checks
- [ ] ERC in schematic passes (no unresolved power pins)
- [ ] DRC in PCB passes with zero unapproved violations
- [ ] HV/LV barrier keepout remains intact (x=60..68mm)
- [ ] Minimum HV clearance/c creepage targets are met per `fab_notes.md`
- [ ] Net continuity checked for: L_IN, N_IN, 5V_MAIN, GND_ISO, UART_TX_M2S, UART_RX_S2M, PHASE_A/B/C

### 2. Safety-critical review
- [ ] Fuse F1 value/type verified for your application
- [ ] MOV1 voltage class verified (275VAC class for 230VAC systems)
- [ ] NTC1 inrush part verified (power rating and surge capability)
- [ ] AC/DC module pinout and footprint verified against datasheet
- [ ] No copper pour crosses isolation barrier
- [ ] HV markings and connector polarity labels are readable on silkscreen

### 3. Mechanical checks
- [ ] Board size and mounting holes fit enclosure
- [ ] USB connector access for both ESP32 modules is unobstructed
- [ ] Terminal block orientation supports wiring entry direction
- [ ] Component heights fit enclosure constraints

### 4. Fabrication outputs
Generate and verify these files:
- [ ] Gerber layers: F.Cu, B.Cu, F.Mask, B.Mask, F.SilkS, B.SilkS, Edge.Cuts
- [ ] Drill files (Excellon)
- [ ] Pick-and-place file (if assembly service is used)
- [ ] BOM export aligned with `bom.csv`
- [ ] Readme/notes for manufacturer mentioning isolation slot and HV spacing requirements

### 5. Visual review in Gerber viewer
- [ ] Edge.Cuts is a closed contour
- [ ] HV traces are not routed through keepout channel
- [ ] Terminal pin numbering matches silkscreen legends
- [ ] No accidental text overlap on pads/vias
- [ ] Mounting holes are free of copper where required

## Recommended KiCad Export Steps

1. Open `master_slave_230v_carrier.kicad_pro`.
2. Run `Inspect -> Design Rules Checker` and clear all critical issues.
3. Open `File -> Fabrication Outputs -> Gerbers` and select required layers.
4. Generate drill files via `File -> Fabrication Outputs -> Drill Files`.
5. Validate all outputs in KiCad Gerber Viewer or an external viewer.
6. Package Gerbers + drill + BOM + fabrication notes into a zip archive.

## First Power-Up Checklist (No ESP32 installed)

- [ ] Apply mains input via isolated bench setup and RCD-protected line
- [ ] Measure secondary output from AC/DC module: expected ~5V
- [ ] Verify no abnormal heating in F1/MOV1/NTC1/PS1
- [ ] Verify 5V_MAIN to GND_ISO is stable under light dummy load

After that:
- [ ] Install Master/Slave ESP32 modules
- [ ] Perform UART and DAC functional tests per `docs/TESTING.md`
