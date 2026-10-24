#pragma once

#include <string>
#include <vector>

struct TimbrePreset {
    std::string name = "Default Preset";
    std::string baseInstrument = "NS3_Bright_Grand"; // Instrument name or filename
    
    // Synth / Tone Filter
    float filterCutoff = 20000.0f; // 20 .. 20000 Hz
    float filterResonance = 0.0f;  // 0.0 .. 0.95
    
    // Envelope (ADSR)
    float attackMs = 5.0f;         // 0 .. 2000 ms
    float decayMs = 250.0f;        // 10 .. 5000 ms
    float sustainLevel = 1.0f;     // 0.0 .. 1.0 (1.0 = full sample natural level)
    float releaseMs = 600.0f;      // 50 .. 5000 ms
    
    // 3-Band Equalizer (Grave, Médio, Agudo)
    bool eqEnabled = true;
    float eqLowGainDb = 0.0f;      // -15 .. +15 dB
    float eqLowFreqHz = 100.0f;    // 20 .. 500 Hz
    float eqMidGainDb = 0.0f;      // -15 .. +15 dB
    float eqMidFreqHz = 1200.0f;   // 200 .. 6000 Hz
    float eqMidQ = 0.707f;         // 0.1 .. 10.0
    float eqHighGainDb = 0.0f;     // -15 .. +15 dB
    float eqHighFreqHz = 8000.0f;  // 2000 .. 18000 Hz
    
    // Studio Compressor
    bool compEnabled = true;
    float compThresholdDb = -14.0f;// -40 .. 0 dB
    float compRatio = 3.5f;        // 1.0 .. 20.0
    float compAttackMs = 15.0f;    // 1 .. 100 ms
    float compReleaseMs = 160.0f;  // 20 .. 1000 ms
    float compMakeupDb = 2.5f;     // 0 .. 20 dB
    float compKneeDb = 4.0f;       // 0 .. 12 dB
    
    // Master
    float masterVolume = 1.0f;     // 0.0 .. 2.0

    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
    std::string serialize() const;
    bool deserialize(const std::string& json);
};
