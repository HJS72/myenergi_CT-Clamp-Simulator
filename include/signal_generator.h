#ifndef SIGNAL_GENERATOR_H
#define SIGNAL_GENERATOR_H

#include <Arduino.h>
#include "config.h"

class ACSignalGenerator {
public:
    ACSignalGenerator();
    
    // Initialize the signal generator
    void begin();
    
    // Update current values (RMS Amperes)
    void setCurrentPhaseA(float amps);
    void setCurrentPhaseB(float amps);
    void setCurrentPhaseC(float amps);
    
    // Generate and output AC signals
    void update();
    
    // Get actual DAC/PWM output values for debugging
    uint8_t getLastDACValueA();
    uint8_t getLastDACValueB();
    uint8_t getLastDACValueC();
    
private:
    // Current values (RMS)
    float currentsRMS[3] = {0.0, 0.0, 0.0};
    
    // Waveform generation
    uint16_t sampleIndex = 0;
    unsigned long lastUpdateTime = 0;
    
    // Last output values for debugging
    uint8_t lastDACValues[3] = {128, 128, 128};  // 128 is mid-point (bias)
    
    // Helper: Generate sine wave with DC offset
    // DC offset (128) allows measurement of negative half-cycle
    uint8_t generateSineWave(float amplitude, uint16_t phase);
    
    // Helper: Output to DAC or PWM
    void outputSignal(uint8_t phaseIndex, uint8_t value);
};

#endif // SIGNAL_GENERATOR_H
