#include "Equalizer.hpp"

Equalizer::Equalizer(float sampleRate) : m_sampleRate(sampleRate) {
    updateFilters();
}

void Equalizer::setSampleRate(float sampleRate) {
    std::lock_guard<std::mutex> lock(m_paramMutex);
    m_sampleRate = sampleRate;
    updateFilters();
}

void Equalizer::reset() {
    std::lock_guard<std::mutex> lock(m_paramMutex);
    for (int ch = 0; ch < 2; ++ch) {
        m_lowFilter[ch].reset();
        m_midFilter[ch].reset();
        m_highFilter[ch].reset();
    }
}

void Equalizer::setLowGain(float gainDb) {
    std::lock_guard<std::mutex> lock(m_paramMutex);
    m_lowGainDb = std::clamp(gainDb, -24.0f, 24.0f);
    updateFilters();
}

void Equalizer::setLowFrequency(float freqHz) {
    std::lock_guard<std::mutex> lock(m_paramMutex);
    m_lowFreq = std::clamp(freqHz, 20.0f, 800.0f);
    updateFilters();
}

void Equalizer::setMidGain(float gainDb) {
    std::lock_guard<std::mutex> lock(m_paramMutex);
    m_midGainDb = std::clamp(gainDb, -24.0f, 24.0f);
    updateFilters();
}

void Equalizer::setMidFrequency(float freqHz) {
    std::lock_guard<std::mutex> lock(m_paramMutex);
    m_midFreq = std::clamp(freqHz, 200.0f, 8000.0f);
    updateFilters();
}

void Equalizer::setMidQ(float q) {
    std::lock_guard<std::mutex> lock(m_paramMutex);
    m_midQ = std::clamp(q, 0.1f, 10.0f);
    updateFilters();
}

void Equalizer::setHighGain(float gainDb) {
    std::lock_guard<std::mutex> lock(m_paramMutex);
    m_highGainDb = std::clamp(gainDb, -24.0f, 24.0f);
    updateFilters();
}

void Equalizer::setHighFrequency(float freqHz) {
    std::lock_guard<std::mutex> lock(m_paramMutex);
    m_highFreq = std::clamp(freqHz, 2000.0f, 18000.0f);
    updateFilters();
}

void Equalizer::updateFilters() {
    for (int ch = 0; ch < 2; ++ch) {
        m_lowFilter[ch].configure(FilterType::LowShelf, m_sampleRate, m_lowFreq, 1.0f, m_lowGainDb);
        m_midFilter[ch].configure(FilterType::Peaking, m_sampleRate, m_midFreq, m_midQ, m_midGainDb);
        m_highFilter[ch].configure(FilterType::HighShelf, m_sampleRate, m_highFreq, 1.0f, m_highGainDb);
    }
}

void Equalizer::process(float* buffer, uint32_t numFrames, uint32_t numChannels) {
    if (!m_enabled || numChannels == 0) return;

    for (uint32_t f = 0; f < numFrames; ++f) {
        if (numChannels >= 2) {
            float inL = buffer[f * numChannels];
            float inR = buffer[f * numChannels + 1];

            // Left
            float outL = m_lowFilter[0].process(inL);
            outL = m_midFilter[0].process(outL);
            outL = m_highFilter[0].process(outL);
            buffer[f * numChannels] = outL;

            // Right
            float outR = m_lowFilter[1].process(inR);
            outR = m_midFilter[1].process(outR);
            outR = m_highFilter[1].process(outR);
            buffer[f * numChannels + 1] = outR;
        } else {
            float inM = buffer[f];
            float outM = m_lowFilter[0].process(inM);
            outM = m_midFilter[0].process(outM);
            outM = m_highFilter[0].process(outM);
            buffer[f] = outM;
        }
    }
}
