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
5. Place C4 close to J3 USB-C input and F1

## USB-C power input (J3)
- Power-only USB-C sink implementation
- CC1 and CC2 each require 5.1k pull-down to GND (RCC1/RCC2)
- Route VBUS through F1 polyfuse before 5V distribution to both ESP32 modules
- Keep USB-C input return path short and wide

## Connector pinout to Harvi/Zappi (J4A/J4B/J4C)
- J4A Pin 1: Phase A DAC (from Master GPIO25 through R1)
- J4A Pin 2: GND
- J4B Pin 1: Phase B DAC (from Slave GPIO25 through R2)
- J4B Pin 2: GND
- J4C Pin 1: Phase C DAC (from Slave GPIO26 through R3)
- J4C Pin 2: GND

## OLED connector pinout (J6)
- J6 Pin 1: 3V3
- J6 Pin 2: GND
- J6 Pin 3: SCL (GPIO22)
- J6 Pin 4: SDA (GPIO21)

Notes:
- Optional pull-up resistors R4/R5 (4.7k) can be populated if display module has weak/no pull-ups.

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
