# PCB Design - Master-Slave Variante mit 230V Eingang

## Ziel
Zweite PCB-Variante mit direktem 230VAC-Eingang und galvanisch isolierter 5V-Versorgung auf derselben Tragerplatine.

## Sicherheitsrahmen
- Diese Variante beinhaltet Netzspannung.
- Aufbau, Inbetriebnahme und Abnahme nur durch qualifizierte Fachkrafte.
- Vor Einsatz sind elektrische Sicherheitsprufungen erforderlich (Isolation, Temperatur, Schutzkonzept).

## Ergebnis im Repo
- [hardware/pcb/master_slave_230v/README.md](hardware/pcb/master_slave_230v/README.md)
- [hardware/pcb/master_slave_230v/netlist.csv](hardware/pcb/master_slave_230v/netlist.csv)
- [hardware/pcb/master_slave_230v/bom.csv](hardware/pcb/master_slave_230v/bom.csv)
- [hardware/pcb/master_slave_230v/placement.csv](hardware/pcb/master_slave_230v/placement.csv)
- [hardware/pcb/master_slave_230v/fab_notes.md](hardware/pcb/master_slave_230v/fab_notes.md)
- [hardware/pcb/master_slave_230v/master_slave_230v_carrier.kicad_pro](hardware/pcb/master_slave_230v/master_slave_230v_carrier.kicad_pro)
- [hardware/pcb/master_slave_230v/master_slave_230v_carrier.kicad_dru](hardware/pcb/master_slave_230v/master_slave_230v_carrier.kicad_dru)
- [hardware/pcb/master_slave_230v/master_slave_230v_carrier.kicad_pcb](hardware/pcb/master_slave_230v/master_slave_230v_carrier.kicad_pcb)
- [hardware/pcb/master_slave_230v/MANUFACTURER_NOTES.md](hardware/pcb/master_slave_230v/MANUFACTURER_NOTES.md)

## Grundkonzept
- HV-Bereich: AC Eingang (L/N/PE), Sicherung, NTC, MOV, AC/DC-Modul
- Isolationsbarriere: Creepage/Clearance + Frasschlitz
- LV-Bereich: Master/Slave ESP32, UART, Phase A/B/C Ausgang

## Elektrisches Mapping (LV)
- Master GPIO25 -> Phase A
- Slave GPIO25 -> Phase C
- Slave GPIO26 -> Phase B
- Master GPIO17 -> Slave GPIO16
- Slave GPIO17 -> Master GPIO16

## Empfohlene Schutzkomponenten
- F1: Primarsicherung in Reihe zu L
- MOV: Uberspannungsschutz zwischen L/N
- NTC: Einschaltstrombegrenzung in L-Pfad
- PS1: Zertifiziertes isoliertes AC/DC-Modul (z. B. HLK-PM01, IRM-05-5)
- F2: Sekundarseitige 5V-Absicherung

## Isolationsregeln (Mindestwerte)
- Primar <-> Sekundar Clearance/Creepage: >= 6mm
- Besser: >= 8mm
- Keine Kupferflachen uber die Isolationsgrenze
- Markierter HV-Bereich im Silkscreen

## Fertigungsparameter
- 2-Lagen FR4, 1.6mm
- 1oz Kupfer
- LV Signale >= 0.25mm
- 5V/GND >= 0.8mm
- HV Leiterbahnen >= 1.2mm empfohlen

## Hinweise zur Inbetriebnahme
1. Zuerst ohne ESP32 bestucken und sekundare 5V messen.
2. Danach Master/Slave einsetzen und UART-Test fahren.
3. Danach Phase-Ausgange im Leerlauf messen (um 1.65V Bias).
4. Erst dann Verbindung zu Harvi/Zappi herstellen.

## Status der KiCad-Boarddatei
- Board-Outline, HV/LV-Trennbereich und Keepout sind gesetzt.
- Kritische Vor-Routings fur Primar- und Sekundarversorgung sind eingetragen.
- Vor-Routing fur PHASE-Ausgangspfad uber R1/R2/R3 ist eingetragen.
- Signal-Matrix ist als Header umgesetzt (MASTER_SIG/SLAVE_SIG), damit die finale Pin-zu-Pin-Zuordnung je nach ESP32-DevKit-Revision sauber gebrückt werden kann.
- Silkscreen ist fur die Bestuckung vervollstandigt:
	- J1 mit L/N/PE
	- J2 mit +5V/GND
	- J5 mit A/B/C/GND
	- J8/J9 Bridge-Pinmapping
	- Schutzteile F1/MOV1/NTC1/PS1/F2 beschriftet

## Umgesetzte feste Bridge-Variante (ESP32 DevKit V1, 30-pin)

Im PCB sind zusatzliche Bridge-Header vorhanden:
- J8: MASTER_DEVKIT_BRIDGE
- J9: SLAVE_DEVKIT_BRIDGE

Empfohlene Drahtbrucken (kurz, isoliert):

Master (J8 -> Master DevKit):
- J8-1 (PHASE_A_DAC) -> GPIO25
- J8-2 (UART_TX_M2S) -> GPIO17
- J8-3 (UART_RX_S2M) -> GPIO16
- J8-4 (GND_ISO) -> GND

Slave (J9 -> Slave DevKit):
- J9-1 (PHASE_B_DAC) -> GPIO26
- J9-2 (PHASE_C_DAC) -> GPIO25
- J9-3 (UART_TX_M2S) -> GPIO16
- J9-4 (UART_RX_S2M) -> GPIO17
- J9-5 (GND_ISO) -> GND

Damit ist die finale Sockel-Zuordnung fur die Standard-DevKit-V1-Bestuckung umgesetzt.

## Wichtiger Hinweis
Dies ist eine Engineering-Basis fur die zweite Variante, keine zertifizierte Serienfreigabe.
