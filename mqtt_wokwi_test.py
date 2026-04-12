#!/usr/bin/env python3
"""
Wokwi MQTT Test - Fixed pattern for CT-Clamp Simulator
  Phase A: constant  +1 A
  Phase B: constant  -2 A
    Phase C: sine ±15 A at 0.02 Hz  (one full cycle every 50 s)

Usage:
    python3 mqtt_wokwi_test.py
    python3 mqtt_wokwi_test.py --interval 2.0   # publish every 2 s (default: 1 s)
"""

import paho.mqtt.client as mqtt
import math
import time
import argparse
import sys

BROKER   = "broker.hivemq.com"
PORT     = 1883
TOPICS   = {
    "A": "home/power/phase_a/current",
    "B": "home/power/phase_b/current",
    "C": "home/power/phase_c/current",
}

PHASE_A_CONSTANT  =  1.0   # A
PHASE_B_CONSTANT  = -2.0   # A
PHASE_C_AMPLITUDE = 15.0   # A  (peak)
PHASE_C_FREQUENCY =  0.02  # Hz → T = 50 s


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✓ Connected to {BROKER}:{PORT}")
    else:
        print(f"✗ Connection failed (rc={rc})")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Send fixed test pattern to Wokwi via MQTT")
    parser.add_argument("--interval", type=float, default=1.0,
                        help="Publish interval in seconds (default: 1.0)")
    args = parser.parse_args()

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION1,
        client_id="wokwi-test"
    )
    client.on_connect = on_connect
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_start()
    time.sleep(1.5)  # wait for connection

    print(f"Publishing every {args.interval}s — Ctrl-C to stop\n")
    print(f"  Phase A : {PHASE_A_CONSTANT:+.1f} A  (constant)")
    print(f"  Phase B : {PHASE_B_CONSTANT:+.1f} A  (constant)")
    print(f"  Phase C : ±{PHASE_C_AMPLITUDE:.1f} A  sine @ {PHASE_C_FREQUENCY} Hz "
          f"(T = {1/PHASE_C_FREQUENCY:.0f} s)\n")

    t_start = time.time()
    try:
        while True:
            t = time.time() - t_start
            c = PHASE_C_AMPLITUDE * math.sin(2 * math.pi * PHASE_C_FREQUENCY * t)

            client.publish(TOPICS["A"], f"{PHASE_A_CONSTANT:.2f}")
            client.publish(TOPICS["B"], f"{PHASE_B_CONSTANT:.2f}")
            client.publish(TOPICS["C"], f"{c:.2f}")

            elapsed_min, elapsed_sec = divmod(int(t), 60)
            print(f"  t={elapsed_min:02d}:{elapsed_sec:02d}  "
                  f"A={PHASE_A_CONSTANT:+.2f}A  "
                  f"B={PHASE_B_CONSTANT:+.2f}A  "
                  f"C={c:+.2f}A")

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\n⚠ Stopped by user")
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
