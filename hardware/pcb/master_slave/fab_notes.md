# Fabrication Notes - Master-Slave Carrier Board

## Board target
- Board size: 120mm x 60mm
- Layers: 2
- Copper: 1oz
- FR4 thickness: 1.6mm
- Surface finish: HASL lead-free (ENIG optional)
- Solder mask: Green (any color works)
- Silkscreen: White

## Electrical constraints
- Trace width (signal): >= 0.25mm
- Trace width (5V/GND main): >= 0.8mm
- Via drill: 0.3mm
- Clearance: >= 0.2mm

## Routing priorities
1. Keep UART traces short and parallel length-balanced
2. Keep DAC traces away from power entry and switching noise
3. Star-ground at power input, then wide ground pour
4. Place C1/C2/C3 close to signal terminal side
5. Place C4 close to J3 input terminal

## Connector pinout to Harvi/Zappi (J4)
- Pin 1: Phase A DAC (from Master GPIO25 through R1)
- Pin 2: Phase B DAC (from Slave GPIO25 through R2)
- Pin 3: Phase C DAC (from Slave GPIO26 through R3)
- Pin 4: GND

## UART interconnect (on-board)
- Master GPIO17 -> Slave GPIO16
- Slave GPIO17 -> Master GPIO16
- Common GND mandatory

## DFM checklist
- Add test pads for: 5V, GND, UART_TX_M2S, UART_RX_S2M, PHASE_A/B/C
- Add board labels on silkscreen for all connector pins
- Keep at least 3mm keepout from board edge for traces
- Validate 3D clearance for ESP32 USB connector access

## Safety note
This board carries low-voltage signals only. Do not connect mains voltage directly.
