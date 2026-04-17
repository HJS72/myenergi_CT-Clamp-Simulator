# PCB Design - Master-Slave Variante

## Ziel
Eine einzige Tragerplatine, auf der beide ESP32-Module sitzen:
- Master links (WiFi/MQTT + Phase A)
- Slave rechts (Phase B/C)
- On-board UART-Verbindung
- Gemeinsamer 5V-Eingang
- Ein Ausgangsstecker A/B/C/GND zum Harvi/Zappi

## Ergebnis im Repo
Die PCB-Dateien liegen hier:
- [hardware/pcb/master_slave/README.md](hardware/pcb/master_slave/README.md)
- [hardware/pcb/master_slave/netlist.csv](hardware/pcb/master_slave/netlist.csv)
- [hardware/pcb/master_slave/bom.csv](hardware/pcb/master_slave/bom.csv)
- [hardware/pcb/master_slave/placement.csv](hardware/pcb/master_slave/placement.csv)
- [hardware/pcb/master_slave/fab_notes.md](hardware/pcb/master_slave/fab_notes.md)

## Elektrisches Mapping
- Master GPIO25 -> Phase A
- Slave GPIO25 -> Phase C
- Slave GPIO26 -> Phase B
- Master GPIO17 -> Slave GPIO16
- Slave GPIO17 -> Master GPIO16
- GND gemeinsam

## Anschlussklemmen
- J3 (2-polig): 5V/GND Eingang
- J4 (4-polig): Phase A / Phase B / Phase C / GND

## Empfohlene PCB-Parameter
- 2-layer FR4
- 1.6mm Dicke
- 1oz Kupfer
- Signal: >= 0.25mm Leiterbahnbreite
- 5V/GND Hauptschienen: >= 0.8mm
- Mindestabstand: >= 0.2mm

## KiCad in 10 Schritten
1. Neues Projekt anlegen
2. Zwei ESP32-Header-Paare platzieren (J1/J2)
3. Schraubklemmen J3/J4 platzieren
4. R1/R2/R3 (10k) in Serie zu A/B/C platzieren
5. C1/C2/C3 (100n) nahe Ausgang platzieren
6. C4 (470uF) nahe 5V-Eingang platzieren
7. UART kreuzverbinden (TX->RX)
8. GND-Polygon unten vollflachig
9. DRC/ERC laufen lassen
10. Gerber + Drill exportieren

## Produktion
Du kannst die Daten bei JLCPCB, PCBWay oder Aisler fertigen lassen.

## Wichtig
Diese Platine ist fur Kleinspannung. Keine direkte Netzspannung auf die Platine fuhren.

## Zweite Variante mit 230V Eingang
Falls du die Versorgung direkt aus 230VAC auf der Platine aufbauen willst, nutze die separate HV/LV-Variante:
- [docs/PCB_MASTER_SLAVE_230V.md](docs/PCB_MASTER_SLAVE_230V.md)
- [hardware/pcb/master_slave_230v/README.md](hardware/pcb/master_slave_230v/README.md)

Diese zweite Variante trennt Netz- und Kleinspannungsbereich klar und nutzt ein isoliertes AC/DC-Modul.
