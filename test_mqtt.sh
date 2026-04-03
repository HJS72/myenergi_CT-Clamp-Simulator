#!/bin/bash
# MQTT Test Script - Send test current values to ESP32

BROKER="192.168.1.100"
PORT="1883"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Myenergi Harvi - MQTT Test Script${NC}\n"

# Check if mosquitto_pub is installed
if ! command -v mosquitto_pub &> /dev/null; then
    echo "Error: mosquitto_pub not found. Install with: brew install mosquitto"
    exit 1
fi

# Test connection
echo "Testing MQTT connection to $BROKER:$PORT..."
mosquitto_pub -h $BROKER -p $PORT -t "test/connection" -m "online" -E
if [ $? -ne 0 ]; then
    echo "Failed to connect to MQTT broker"
    exit 1
fi
echo -e "${GREEN}✓ Connected${NC}\n"

# Send some test values
echo "Sending test current values..."

# Test sequence: ramp up, steady, ramp down
test_values=("5" "10" "15" "20" "25" "30" "25" "20" "15" "10" "5" "0")

for value in "${test_values[@]}"; do
    echo -e "  Phase A: ${GREEN}$value A${NC}"
    mosquitto_pub -h $BROKER -p $PORT -t "home/power/phase_a/current" -m "$value"
    
    # Phase B shifted by ~90 degrees in amplitude
    pb_value=$(echo "$value * 0.7" | bc)
    echo -e "  Phase B: ${GREEN}$pb_value A${NC}"
    mosquitto_pub -h $BROKER -p $PORT -t "home/power/phase_b/current" -m "$pb_value"
    
    # Phase C shifted
    pc_value=$(echo "$value * 0.5" | bc)
    echo -e "  Phase C: ${GREEN}$pc_value A${NC}"
    mosquitto_pub -h $BROKER -p $PORT -t "home/power/phase_c/current" -m "$pc_value"
    
    sleep 1
done

echo -e "\n${GREEN}✓ Test complete${NC}"
