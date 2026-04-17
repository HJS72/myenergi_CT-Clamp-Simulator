#!/usr/bin/env python3
"""
Debug script to check what values are retained in MQTT broker
"""

import paho.mqtt.client as mqtt
import sys
import time

broker = "10.13.30.201"
port = 1885
device_short_name = "150_194_ESP32CTSimulator"

received_messages = {}

def on_message(client, userdata, msg):
    topic = msg.topic
    value = msg.payload.decode()
    received_messages[topic] = value
    print(f"  {topic}: {value} (retain={msg.retain})")

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✓ Connected to {broker}:{port}\n")
        print("Subscribing to phase topics...")
        client.subscribe(f"/{device_short_name}/PhaseA_Amp")
        client.subscribe(f"/{device_short_name}/PhaseB_Amp")
        client.subscribe(f"/{device_short_name}/PhaseC_Amp")
        client.subscribe(f"/{device_short_name}/Status")
    else:
        print(f"✗ Connection failed: {rc}")
        sys.exit(1)

# Create client
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect(broker, port, keepalive=60)
    client.loop_start()
    time.sleep(2)
    
    print("\n📊 Current retained values in broker:")
    if received_messages:
        for topic, value in sorted(received_messages.items()):
            print(f"  {topic}: {value}")
    else:
        print("  (no messages received)")
    
    client.loop_stop()
    client.disconnect()
    
except Exception as e:
    print(f"✗ Error: {e}")
    sys.exit(1)
