#include "signal_generator.h"

ACSignalGenerator::ACSignalGenerator() {}

void ACSignalGenerator::begin() {
    #if DAC_ENABLED
    // Initialize DAC output
    pinMode(DAC_PIN_PHASE_A, OUTPUT);
    pinMode(DAC_PIN_PHASE_B, OUTPUT);
    
    // Set initial values (middle = 128 for AC with bias)
    dacWrite(DAC_PIN_PHASE_A, 128);
    dacWrite(DAC_PIN_PHASE_B, 128);
    #endif
    
    #if PWM_ENABLED
    // Initialize PWM
    ledcSetup(0, 1000, 8);  // Channel 0, 1kHz, 8-bit
    ledcAttachPin(DAC_PIN_PHASE_A, 0);
    #endif
}

void ACSignalGenerator::setCurrentPhaseA(float amps) {
    currentsRMS[0] = constrain(amps, 0.0, (float)CURRENT_MAX);
}

void ACSignalGenerator::setCurrentPhaseB(float amps) {
    currentsRMS[1] = constrain(amps, 0.0, (float)CURRENT_MAX);
}

void ACSignalGenerator::setCurrentPhaseC(float amps) {
    currentsRMS[2] = constrain(amps, 0.0, (float)CURRENT_MAX);
}

void ACSignalGenerator::update() {
    unsigned long now = millis();
    
    // Update at specified interval
    if (now - lastUpdateTime < UPDATE_INTERVAL) {
        return;
    }
    lastUpdateTime = now;
    
    // Calculate indices for 3-phase (120 degree shifts)
    uint16_t indexA = sampleIndex;
    uint16_t indexB = (sampleIndex + (SIGNAL_SAMPLES / 3)) % SIGNAL_SAMPLES;
    uint16_t indexC = (sampleIndex + (2 * SIGNAL_SAMPLES / 3)) % SIGNAL_SAMPLES;
    
    // Generate AC signals with current-dependent amplitude
    uint8_t valueA = generateSineWave(currentsRMS[0], indexA);
    uint8_t valueB = generateSineWave(currentsRMS[1], indexB);
    uint8_t valueC = generateSineWave(currentsRMS[2], indexC);
    
    // Output to DAC or PWM
    outputSignal(0, valueA);
    outputSignal(1, valueB);
    outputSignal(2, valueC);
    
    lastDACValues[0] = valueA;
    lastDACValues[1] = valueB;
    lastDACValues[2] = valueC;
    
    // Move to next sample
    sampleIndex = (sampleIndex + 1) % SIGNAL_SAMPLES;
}

uint8_t ACSignalGenerator::generateSineWave(float amplitude, uint16_t phase) {
    // Convert phase index to radians
    float radians = (2.0 * PI * phase) / SIGNAL_SAMPLES;
    
    // Generate sine wave
    // Range: -1 to +1
    float sine = sin(radians);
    
    // Scale by current amplitude
    float scaled = sine * amplitude;
    
    // Convert to DAC value with DC offset (128)
    // DAC range: 0-255, middle (128) represents 0A
    // Amplitude scaled to fit in range: ±(128 * amplitude / CURRENT_MAX)
    float dacValue = 128.0 + scaled * (128.0 / CURRENT_MAX);
    
    // Constrain to valid range
    uint8_t result = (uint8_t)constrain(dacValue, 0, 255);
    
    return result;
}

void ACSignalGenerator::outputSignal(uint8_t phaseIndex, uint8_t value) {
    #if DAC_ENABLED
    if (phaseIndex == 0) {
        dacWrite(DAC_PIN_PHASE_A, value);
    } else if (phaseIndex == 1) {
        dacWrite(DAC_PIN_PHASE_B, value);
    }
    #endif
    
    #if PWM_ENABLED
    if (phaseIndex == 0) {
        ledcWrite(0, value);
    }
    #endif
}

uint8_t ACSignalGenerator::getLastDACValueA() {
    return lastDACValues[0];
}

uint8_t ACSignalGenerator::getLastDACValueB() {
    return lastDACValues[1];
}

uint8_t ACSignalGenerator::getLastDACValueC() {
    return lastDACValues[2];
}
