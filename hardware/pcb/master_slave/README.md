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
- Slave GPIO25 -> Phase C
- Slave GPIO26 -> Phase B
- Master GPIO17 -> Slave GPIO16
- Slave GPIO17 -> Master GPIO16
- Master GPIO32 -> Slave EN (serial pass-through OTA control)
- Master GPIO33 -> Slave GPIO0 (serial pass-through OTA boot control)

## Serial Pass-Through OTA Wiring

Required additional control wires for flashing the slave through the master web UI:

- `Master GPIO32` -> `Slave EN`
- `Master GPIO33` -> `Slave GPIO0`

Recommended implementation:

- Drive both control lines through NPN transistor stages or open-drain MOSFETs.
- Keep Slave EN pulled up (10k to 3V3).
- Keep Slave GPIO0 pulled up (10k to 3V3).
- Ensure Master and Slave share a low-impedance common ground.

The firmware uses these lines to reset the slave and force boot mode before UART image transfer.

## DAC Output Wiring (Production)

Use the following direct mapping from ESP32 DAC pins to output terminal blocks:

- Phase A (Master DAC)
	- Source: `J1A-9_IO25` (Master GPIO25 / DAC1)
	- Net: `PHASE_A_DAC`
	- Destination: `J4A-1` (Phase A output)
	- Return: `J4A-2` -> `GND`

- Phase B (Slave DAC)
	- Source: `J2A-10_IO26` (Slave GPIO26 / DAC2)
	- Net: `PHASE_B_DAC`
	- Destination: `J4B-1` (Phase B output)
	- Return: `J4B-2` -> `GND`

- Phase C (Slave DAC)
	- Source: `J2A-9_IO25` (Slave GPIO25 / DAC1)
	- Net: `PHASE_C_DAC`
	- Destination: `J4C-1` (Phase C output)
	- Return: `J4C-2` -> `GND`

Optional analog smoothing/filter per phase:

- Series resistor in output path:
	- `R1` on Phase A
	- `R2` on Phase B
	- `R3` on Phase C
- Shunt capacitor from phase net to GND:
	- `C1` on Phase A
	- `C2` on Phase B
	- `C3` on Phase C

This creates a simple RC low-pass per DAC channel and can reduce high-frequency DAC noise before the external interface stage.

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

## ASCII Wiring Schema (Production)

```text
Power entry and distribution
----------------------------
 USB-C J3
	VBUS ----> F1 (polyfuse) ----> 5V_IN ----> J1A-19 (Master 5V)
														+-> J2A-19 (Slave 5V)

	GND  ----------------------------------> J1B-1 (Master GND)
		+-----------------------------------> J2B-1 (Slave GND)

	CC1 ----> RCC1 5.1k ----> GND
	CC2 ----> RCC2 5.1k ----> GND


Master/Slave interconnect
-------------------------
 UART_TX_M2S:  J1B-11 (Master IO17 TX) ----> J2B-12 (Slave IO16 RX)
 UART_RX_S2M:  J2B-11 (Slave  IO17 TX) ----> J1B-12 (Master IO16 RX)
 SLAVE_EN_CTL: J1A-7  (Master IO32)    ----> J2A-2  (Slave EN)
 SLAVE_BOOT:   J1A-8  (Master IO33)    ----> J2B-14 (Slave IO0)
 Common GND:   J1B-1 -----------------------> J2B-1


OLED header J6 (optional)
-------------------------
 J6-1 (VCC) <---- J1A-1  (Master 3V3)
 J6-2 (GND) <---- J1B-1  (Master GND)
 J6-3 (SCL) <---- J1B-3  (Master IO22)
 J6-4 (SDA) <---- J1B-6  (Master IO21)

 Optional pull-ups:
	R4: J6-3 (SCL) -> J6-1 (3V3)
	R5: J6-4 (SDA) -> J6-1 (3V3)


Phase outputs to external terminal blocks
-----------------------------------------
 PHASE_A_DAC: J1A-9  (Master IO25) ----> J4A-1 (Phase A)
 PHASE_B_DAC: J2A-10 (Slave  IO26) ----> J4B-1 (Phase B)
 PHASE_C_DAC: J2A-9  (Slave  IO25) ----> J4C-1 (Phase C)

 Ground returns:
	J4A-2 -> GND
	J4B-2 -> GND
	J4C-2 -> GND


Optional enable/reset switches
------------------------------
 SW1-1 ----> J1A-2 (Master EN)
 SW2-1 ----> J2A-2 (Slave EN)
```
