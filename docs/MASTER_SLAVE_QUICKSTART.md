# Master-Slave Quick Start

Use this flow when the master handles WiFi, MQTT and the web UI, while the slave generates Phase B and Phase C.

## Step 1: Flash The Master

```bash
cd myenergi_CT-Clamp-Simulator
.venv/bin/pio run -e esp32-master -t upload
.venv/bin/pio run -e esp32-master -t monitor
```

## Step 2: Flash The Slave

```bash
.venv/bin/pio run -e esp32-slave -t upload
.venv/bin/pio run -e esp32-slave -t monitor
```

## Step 3: Wire UART And OTA Control

```text
Master GPIO17  -> Slave GPIO16
Master GPIO16  <- Slave GPIO17
Master GPIO32  -> Slave EN
Master GPIO33  -> Slave GPIO0
Master GND     -> Slave GND
```

## Step 4: Bring Up The Web UI

- Open the master IP in your browser
- Or join the fallback AP `CTSimulator` and open `http://192.168.4.1/`

Configure WiFi and MQTT there if needed.

## Step 5: Publish Test Data

```bash
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseA_Amp" -m "25"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseB_Amp" -m "23"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseC_Amp" -m "24"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/SumPower_kW" -m "8.7"
```

## Step 6: Verify Runtime State

In the master web UI:

- WiFi is `online`
- MQTT is `online`
- Slave becomes `online`
- `Online since` increments while the slave stays connected
- `Last Change` updates after each MQTT update

On the OLED:

- Phase and power values update
- MQTT and slave indicators show the current comms state

## If Something Fails

### Slave stays offline

- Re-check TX and RX crossover
- Confirm common ground
- Confirm both boards run the correct environment

### MQTT stays offline

- Check broker address, port and credentials in the web UI
- Verify the publish path matches the normalized MQTT path

### Slave OTA is blocked

- Run the slave self-test in the web UI first
- Verify EN and GPIO0 wiring from the master to the slave