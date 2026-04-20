#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <Arduino.h>
#include <cmath>
#include <cstring>

/**
 * UART Protocol for Master-Slave Communication
 * 
 * Packet Format:
 * [SYNC] [LEN] [Phase_B] [Phase_C] [CRC16_L] [CRC16_H] [END]
 * 
 * SYNC:      0x55 (start marker)
 * LEN:       2 (fixed payload length)
 * Phase_B:   uint8_t (0-255, maps to -CURRENT_MAX..+CURRENT_MAX)
 * Phase_C:   uint8_t (0-255, maps to -CURRENT_MAX..+CURRENT_MAX)
 * CRC16:     16-bit checksum
 * END:       0xAA (end marker)
 * 
 * Total: 7 bytes
 */

#define UART_SYNC 0x55
#define UART_END 0xAA
#define UART_PACKET_SIZE 7

struct UARTPacket {
    uint8_t sync;        // 0x55
    uint8_t len;         // 2
    uint8_t phase_b;     // 0-255 (signed current encoded)
    uint8_t phase_c;     // 0-255 (signed current encoded)
    uint8_t crc_low;     // CRC16 low byte
    uint8_t crc_high;    // CRC16 high byte
    uint8_t end;         // 0xAA
    
    UARTPacket() : sync(UART_SYNC), len(2), phase_b(128), phase_c(128), 
                   crc_low(0), crc_high(0), end(UART_END) {}
};

class UARTProtocol {
public:
    static uint8_t encodeSignedCurrent(float amps) {
        const float clamped = constrain(amps, -(float)CURRENT_MAX, (float)CURRENT_MAX);
        const float normalized = (clamped + (float)CURRENT_MAX) / (2.0f * (float)CURRENT_MAX);
        return (uint8_t)constrain((int)lroundf(normalized * 255.0f), 0, 255);
    }

    static float decodeSignedCurrent(uint8_t encoded) {
        const float normalized = (float)encoded / 255.0f;
        const float amps = (normalized * 2.0f - 1.0f) * (float)CURRENT_MAX;
        return constrain(amps, -(float)CURRENT_MAX, (float)CURRENT_MAX);
    }

    /**
     * Calculate CRC16 (CCITT-FALSE polynomial)
     */
    static uint16_t calculateCRC16(const uint8_t* data, size_t length) {
        uint16_t crc = 0xFFFF;
        
        for (size_t i = 0; i < length; i++) {
            crc ^= (uint16_t)data[i] << 8;
            
            for (int j = 0; j < 8; j++) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc = crc << 1;
                }
                crc &= 0xFFFF;
            }
        }
        
        return crc;
    }
    
    /**
     * Create and serialize a packet
     */
    static void createPacket(UARTPacket& packet, float phaseB, float phaseC) {
        // Convert signed currents to uint8_t using bipolar mapping.
        packet.phase_b = encodeSignedCurrent(phaseB);
        packet.phase_c = encodeSignedCurrent(phaseC);
        
        // Calculate CRC over payload (phase_b, phase_c)
        uint8_t payload[2] = {packet.phase_b, packet.phase_c};
        uint16_t crc = calculateCRC16(payload, 2);
        
        packet.crc_low = crc & 0xFF;
        packet.crc_high = (crc >> 8) & 0xFF;
    }
    
    /**
     * Verify and parse a received packet
     */
    static bool parsePacket(const uint8_t* buffer, UARTPacket& packet, 
                           float& phaseB_out, float& phaseC_out) {
        if (buffer[0] != UART_SYNC || buffer[6] != UART_END) {
            return false;  // Invalid markers
        }
        
        // Verify CRC
        uint8_t payload[2] = {buffer[2], buffer[3]};
        uint16_t expectedCRC = calculateCRC16(payload, 2);
        uint16_t receivedCRC = (buffer[5] << 8) | buffer[4];
        
        if (expectedCRC != receivedCRC) {
            return false;  // CRC mismatch
        }
        
        // Parse packet
        packet.sync = buffer[0];
        packet.len = buffer[1];
        packet.phase_b = buffer[2];
        packet.phase_c = buffer[3];
        packet.crc_low = buffer[4];
        packet.crc_high = buffer[5];
        packet.end = buffer[6];
        
        // Convert encoded bytes back to signed current values.
        phaseB_out = decodeSignedCurrent(packet.phase_b);
        phaseC_out = decodeSignedCurrent(packet.phase_c);
        
        return true;
    }
    
    /**
     * Serialize packet to byte array
     */
    static void serialize(const UARTPacket& packet, uint8_t* buffer, size_t bufSize) {
        if (bufSize < UART_PACKET_SIZE) return;
        
        buffer[0] = packet.sync;
        buffer[1] = packet.len;
        buffer[2] = packet.phase_b;
        buffer[3] = packet.phase_c;
        buffer[4] = packet.crc_low;
        buffer[5] = packet.crc_high;
        buffer[6] = packet.end;
    }
};

#endif // UART_PROTOCOL_H
