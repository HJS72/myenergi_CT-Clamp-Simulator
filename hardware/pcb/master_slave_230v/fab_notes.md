# Fabrication Notes - Master-Slave Carrier Board (230VAC Input Variant)

## Critical safety scope
This variant includes mains input handling. Build and commissioning should be done only by qualified personnel.

## Board target
- Board size: 126mm x 66mm
- Layers: 2
- FR4 thickness: 1.6mm
- Copper: 1oz
- Surface finish: ENIG preferred (HASL LF acceptable)

## Isolation and creepage
- Maintain minimum 6mm clearance/creepage between primary (HV) and secondary (LV) zones
- Prefer 8mm where possible
- Add milled isolation slot between HV and LV domains
- No copper pour crossing isolation barrier

## High-voltage section (Primary)
- J1: L/N/PE input
- F1: line fuse in series with L
- NTC1: inrush limiter in line path
- MOV1: across L/N after fuse
- PS1: certified isolated AC/DC module (HLK-PM01 or IRM-05-5 class)

## Low-voltage section (Secondary)
- F2: 1A polyfuse on 5V rail
- ESP32 master/slave sockets and signal path
- UART routing short and away from HV edges

## Routing constraints
- Primary traces: >= 1.2mm preferred
- 5V/GND main: >= 0.8mm
- Signal traces: >= 0.25mm
- Via drill: >= 0.3mm
- Clearance (LV): >= 0.2mm

## Mechanical and labeling
- Clearly mark HV area with silkscreen warning
- Mark L/N/PE and A/B/C/GND terminal pins explicitly
- Keep mounting holes out of HV clearance keepout

## Compliance note
This file is an engineering starting point, not a certified design package. Perform insulation, hipot, thermal, and EMC validation before deployment.
