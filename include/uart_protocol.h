#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <Arduino.h>
#include <cstring>

/**
 * UART Protocol for Master-Slave Communication
 * 
 * Packet Format:
 * [SYNC] [LEN] [Phase_B] [Phase_C] [CRC16_L] [CRC16_H] [END]
 * 
 * SYNC:      0x55 (start marker)
 * LEN:       2 (fixed payload length)
 * Phase_B:   uint8_t (0-255, maps to 0-100A)
 * Phase_C:   uint8_t (0-255, maps to 0-100A)
 * CRC16:     16-bit checksum
 * END:       0xAA (end marker)
 * 
 * Total: 8 bytes
 */

#define UART_SYNC 0x55
#define UART_END 0xAA
#define UART_PACKET_SIZE 8

struct UARTPacket {
    uint8_t sync;        // 0x55
    uint8_t len;         // 2
    uint8_t phase_b;     // 0-255
    uint8_t phase_c;     // 0-255
    uint8_t crc_low;     // CRC16 low byte
    uint8_t crc_high;    // CRC16 high byte
    uint8_t end;         // 0xAA
    
    UARTPacket() : sync(UART_SYNC), len(2), phase_b(128), phase_c(128), 
                   crc_low(0), crc_high(0), end(UART_END) {}
};

class UARTProtocol {
public:
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
        // Convert floats (0-100A) to uint8_t (0-255)
        packet.phase_b = (uint8_t)constrain((phaseB * 255.0 / 100.0), 0, 255);
        packet.phase_c = (uint8_t)constrain((phaseC * 255.0 / 100.0), 0, 255);
        
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
        
        // Convert uint8_t (0-255) to floats (0-100A)
        phaseB_out = (packet.phase_b * 100.0) / 255.0;
        phaseC_out = (packet.phase_c * 100.0) / 255.0;
        
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
