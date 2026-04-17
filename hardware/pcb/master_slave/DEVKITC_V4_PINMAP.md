# ESP32 DevKitC V4 NodeMCU Pinmap (38-pin, 2x19)

This table documents the pin numbering convention used by this PCB project.

Important:
- Verify against the silkscreen text on your exact module revision before final fabrication.
- Numbering direction here is from USB connector side to the opposite end on each header row.

## Header naming in this project
- J1A/J1B: Master module socket rows
- J2A/J2B: Slave module socket rows
- A-row and B-row both use pin numbers 1..19

## Reference pin map (per module)

| Pin # | A-row signal | B-row signal |
|---|---|---|
| 1 | 3V3 | GND |
| 2 | EN | IO23 |
| 3 | SVP / IO36 | IO22 |
| 4 | SVN / IO39 | TX0 / IO1 |
| 5 | IO34 | RX0 / IO3 |
| 6 | IO35 | IO21 |
| 7 | IO32 | GND |
| 8 | IO33 | IO19 |
| 9 | IO25 | IO18 |
| 10 | IO26 | IO5 |
| 11 | IO27 | IO17 |
| 12 | IO14 | IO16 |
| 13 | IO12 | IO4 |
| 14 | GND | IO0 |
| 15 | IO13 | IO2 |
| 16 | SD2 | IO15 |
| 17 | SD3 | SD1 |
| 18 | CMD | SD0 |
| 19 | 5V | CLK |

## Nets used by this project

| Function | Master node | Slave node |
|---|---|---|
| 5V input | J1A-19_5V | J2A-19_5V |
| Ground reference | J1B-1_GND | J2B-1_GND |
| UART TX -> RX | J1B-11_IO17 | J2B-12_IO16 |
| UART RX <- TX | J1B-12_IO16 | J2B-11_IO17 |
| Slave EN control (OTA) | J1A-7_IO32 | J2A-2_EN |
| Slave BOOT control (OTA) | J1A-8_IO33 | J2B-14_IO0 |
| Phase A DAC | J1A-9_IO25 | - |
| Phase B DAC | - | J2A-10_IO26 |
| Phase C DAC | - | J2A-9_IO25 |
| EN switch | J1A-2_EN | J2A-2_EN |

## OLED I2C connector mapping (J6)

| J6 pin | Signal | Master node |
|---|---|---|
| 1 | 3V3 | J1A-1_3V3 |
| 2 | GND | J1B-1_GND |
| 3 | SCL | J1B-3_IO22 |
| 4 | SDA | J1B-6_IO21 |

## Footprint note
Use two 1x19 sockets per module:
- PinSocket_1x19_P2.54mm (A-row)
- PinSocket_1x19_P2.54mm (B-row)
