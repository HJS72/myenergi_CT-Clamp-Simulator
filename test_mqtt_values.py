#!/usr/bin/env python3
"""
Test script to publish non-zero current values to the correct MQTT topics
"""

import paho.mqtt.client as mqtt
import sys
import time

broker = "10.13.30.201"
port = 1885
device_short_name = "150_194_ESP32CTSimulator"

# Create client
client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✓ Connected to {broker}:{port}")
    else:
        print(f"✗ Connection failed: {rc}")
        sys.exit(1)

client.on_connect = on_connect

try:
    client.connect(broker, port, keepalive=60)
    client.loop_start()
    time.sleep(0.5)
    
    # Publish test values with retain flag
    test_values = [
        (f"/{device_short_name}/PhaseA_Amp", "15.5"),
        (f"/{device_short_name}/PhaseB_Amp", "14.2"),
        (f"/{device_short_name}/PhaseC_Amp", "16.8"),
    ]
    
    print("\n📤 Publishing test values to MQTT broker...")
    for topic, value in test_values:
        client.publish(topic, value, retain=True)
        print(f"  Published {topic} = {value}A (retained)")
    
    time.sleep(1)
    client.loop_stop()
    client.disconnect()
    print("\n✓ Done! Check the display after reboot.")
    
except Exception as e:
    print(f"✗ Error: {e}")
    sys.exit(1)
