#pragma once

#include "Biquad.hpp"
#include <cstdint>
#include <mutex>

class Equalizer {
public:
    Equalizer(float sampleRate = 44100.0f);

    void setSampleRate(float sampleRate);
    void reset();

    // Grave (Low Shelf)
    void setLowGain(float gainDb);
    float getLowGain() const { return m_lowGainDb; }
    void setLowFrequency(float freqHz);
    float getLowFrequency() const { return m_lowFreq; }

    // Médio (Parametric Peak)
    void setMidGain(float gainDb);
    float getMidGain() const { return m_midGainDb; }
    void setMidFrequency(float freqHz);
    float getMidFrequency() const { return m_midFreq; }
    void setMidQ(float q);
    float getMidQ() const { return m_midQ; }

    // Agudo (High Shelf)
    void setHighGain(float gainDb);
    float getHighGain() const { return m_highGainDb; }
    void setHighFrequency(float freqHz);
    float getHighFrequency() const { return m_highFreq; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void process(float* buffer, uint32_t numFrames, uint32_t numChannels);

private:
    void updateFilters();

    float m_sampleRate = 44100.0f;
    bool m_enabled = true;

    float m_lowGainDb = 0.0f;
    float m_lowFreq = 100.0f;

    float m_midGainDb = 0.0f;
    float m_midFreq = 1200.0f;
    float m_midQ = 0.707f;

    float m_highGainDb = 0.0f;
    float m_highFreq = 8000.0f;

    // Stereo Biquad filters (Left & Right)
    Biquad m_lowFilter[2];
    Biquad m_midFilter[2];
    Biquad m_highFilter[2];

    std::mutex m_paramMutex;
};
