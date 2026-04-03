# Manufacturer Notes - Master-Slave 230VAC Board

## Project
- Board: Master-Slave ESP32 Carrier (230VAC input variant)
- Location: hardware/pcb/master_slave_230v

## Fabrication Requirements
- PCB type: FR4, 2 layers
- Thickness: 1.6mm
- Copper: 1oz
- Surface finish: ENIG preferred (HASL LF acceptable)
- Solder mask: Standard (green or equivalent)
- Silkscreen: White

## Critical Safety Geometry
- High-voltage and low-voltage sections are separated by dedicated keepout.
- Isolation channel / separation zone is mandatory and must not be modified.
- Maintain requested clearance/creepage per design rules and fab notes.

## Files to Use
- master_slave_230v_carrier.kicad_pcb
- master_slave_230v_carrier.kicad_pro
- master_slave_230v_carrier.kicad_dru
- bom.csv
- netlist.csv
- placement.csv
- fab_notes.md

## Do Not Change Without Approval
- Connector pin order labels on silkscreen (J1/J2/J5/J8/J9)
- HV/LV isolation area and keepout limits
- Net names and trace intent for L/N and 5V isolation path

## Recommended Output Package (from CAD)
- Gerber layers: F.Cu, B.Cu, F.Mask, B.Mask, F.SilkS, B.SilkS, Edge.Cuts
- Drill files: Excellon
- BOM and Pick&Place (if assembly requested)

## Assembly Notes
- This board includes mains-voltage domain. Assembly and test by qualified personnel only.
- Verify AC/DC module footprint and pin-1 orientation against module datasheet before assembly.
- Verify fuse, MOV, NTC ratings according to local mains and protection strategy.

## Incoming Inspection Checklist
- Board outline and hole positions correct
- No mask/silk overlap on pads
- Isolation region intact and free of copper bridges
- Silkscreen labels readable at J1/J2/J5/J8/J9

## First Power Test (No ESP32 installed)
- Apply controlled 230VAC via protected bench setup
- Verify 5V output present and stable
- Check no abnormal heating at F1/MOV1/NTC1/PS1

## Liability / Compliance
- This is an engineering prototype design package, not a certified end-product.
- Final compliance, safety testing, and regulatory approval remain with the integrator.
