#!/usr/bin/env python3
"""
MQTT Test Generator - publish test values to the current MQTT path layout.

Usage:
    python3 mqtt_test_generator.py --broker 192.168.1.100 --path esp32CTSimulator --current 25
"""

import paho.mqtt.client as mqtt
import argparse
import time
import math
import sys

class MQTTTestGenerator:
    def __init__(self, broker, port=1883, mqtt_path="esp32CTSimulator", client_id="mqtt-test-gen"):
        self.broker = broker
        self.port = port
        self.mqtt_path = mqtt_path.strip("/") or "esp32CTSimulator"
        self.client_id = client_id
        self.client = mqtt.Client(client_id=client_id)
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.connected = False
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print(f"✓ Connected to MQTT broker at {self.broker}:{self.port}")
        else:
            print(f"✗ Connection failed with code {rc}")
            
    def on_disconnect(self, client, userdata, rc):
        if rc != 0:
            print(f"Unexpected disconnection: {rc}")
        self.connected = False
        
    def connect(self):
        try:
            self.client.connect(self.broker, self.port, keepalive=60)
            self.client.loop_start()
            time.sleep(1)
            return self.connected
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False
            
    def disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()
        
    def send_current(self, phase_a, phase_b=None, phase_c=None):
        """Send current values for all three phases using the active topic path."""
        if phase_b is None:
            phase_b = phase_a * 0.9
        if phase_c is None:
            phase_c = phase_a * 0.8
            
        topics = [
            (f"/{self.mqtt_path}/PhaseA_Amp", phase_a),
            (f"/{self.mqtt_path}/PhaseB_Amp", phase_b),
            (f"/{self.mqtt_path}/PhaseC_Amp", phase_c),
        ]
        
        for topic, value in topics:
            self.client.publish(topic, f"{value:.2f}")
            
        print(f"  Phase A: {phase_a:.2f}A | Phase B: {phase_b:.2f}A | Phase C: {phase_c:.2f}A")
        
    def test_ramp(self, max_current=50, duration=10):
        """Ramp current up and down"""
        print(f"\n📊 Running ramp test ({max_current}A over {duration}s)...")
        
        steps = 20
        step_duration = duration / steps
        
        # Ramp up
        for i in range(steps):
            current = (i / steps) * max_current
            self.send_current(current)
            time.sleep(step_duration)
            
        # Ramp down
        for i in range(steps, 0, -1):
            current = (i / steps) * max_current
            self.send_current(current)
            time.sleep(step_duration)
            
        self.send_current(0)
        print("✓ Test complete")
        
    def test_sine_wave(self, amplitude=30, frequency=0.2, duration=10):
        """Generate sine wave current"""
        print(f"\n📊 Running sine wave test ({amplitude}A amplitude, {frequency}Hz)...")
        
        samples = int(duration / 0.1)  # 100ms intervals
        for i in range(samples):
            angle = 2 * math.pi * frequency * i * 0.1
            base_current = amplitude / 2 + (amplitude / 2) * math.sin(angle)
            
            phase_a = max(0, base_current)
            phase_b = max(0, (amplitude / 2) + (amplitude / 2) * math.sin(angle + 2*math.pi/3))
            phase_c = max(0, (amplitude / 2) + (amplitude / 2) * math.sin(angle + 4*math.pi/3))
            
            self.send_current(phase_a, phase_b, phase_c)
            time.sleep(0.1)
            
        self.send_current(0)
        print("✓ Test complete")
        
    def test_constant(self, current=25, duration=10):
        """Hold constant current"""
        print(f"\n📊 Holding constant current ({current}A for {duration}s)...")
        
        end_time = time.time() + duration
        while time.time() < end_time:
            self.send_current(current)
            time.sleep(0.5)
            
        self.send_current(0)
        print("✓ Test complete")

def main():
    parser = argparse.ArgumentParser(
        description="Send test MQTT current values to CT Clamp Harvi simulator"
    )
    parser.add_argument("--broker", default="192.168.1.100", help="MQTT broker address")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("--path", default="esp32CTSimulator", help="MQTT topic path without leading slash")
    parser.add_argument("--test", choices=["ramp", "sine", "constant"], default="ramp",
                       help="Test type to run")
    parser.add_argument("--current", type=float, default=50, help="Current amplitude (Amperes)")
    parser.add_argument("--duration", type=float, default=10, help="Test duration (seconds)")
    parser.add_argument("--frequency", type=float, default=0.2, help="Sine wave frequency (Hz)")
    
    args = parser.parse_args()
    
    # Create generator
    gen = MQTTTestGenerator(args.broker, args.port, args.path)
    
    if not gen.connect():
        sys.exit(1)
    
    try:
        print(f"Testing with broker: {args.broker}:{args.port} | path: /{gen.mqtt_path}\n")
        
        if args.test == "ramp":
            gen.test_ramp(args.current, args.duration)
        elif args.test == "sine":
            gen.test_sine_wave(args.current, args.frequency, args.duration)
        elif args.test == "constant":
            gen.test_constant(args.current, args.duration)
            
    except KeyboardInterrupt:
        print("\n⚠ Test interrupted")
    finally:
        gen.disconnect()

if __name__ == "__main__":
    main()
