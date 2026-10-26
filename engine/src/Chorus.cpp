#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include "Chorus.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Chorus::Chorus(float sampleRate) {
    prepare(sampleRate);
}

void Chorus::prepare(float sampleRate) {
    m_sampleRate = sampleRate;
    size_t maxDelay = static_cast<size_t>(m_sampleRate * 0.05f); // 50ms buffer
    m_delayBufferL.assign(maxDelay, 0.0f);
    m_delayBufferR.assign(maxDelay, 0.0f);
    m_writeIdx = 0;
}

void Chorus::reset() {
    std::fill(m_delayBufferL.begin(), m_delayBufferL.end(), 0.0f);
    std::fill(m_delayBufferR.begin(), m_delayBufferR.end(), 0.0f);
    m_writeIdx = 0;
    m_phaseL = 0.0f;
    m_phaseR = 0.25f;
}

void Chorus::setRate(float rateHz) {
    m_rate = std::clamp(rateHz, 0.1f, 8.0f);
}

void Chorus::setDepth(float depthMs) {
    m_depth = std::clamp(depthMs, 0.5f, 15.0f);
}

void Chorus::setMix(float mix) {
    m_mix = std::clamp(mix, 0.0f, 1.0f);
}

void Chorus::process(float* buffer, uint32_t numFrames, uint32_t numChannels) {
    if (!m_enabled || m_mix <= 0.001f || numChannels == 0) return;

    const float mix = m_mix;
    size_t bufSize = m_delayBufferL.size();
    float phaseInc = (m_rate / m_sampleRate);
    float baseDelaySamples = m_depth * 0.001f * m_sampleRate;
    float modAmplitude = baseDelaySamples * 0.7f;

    for (uint32_t f = 0; f < numFrames; ++f) {
        float inL = (numChannels >= 2) ? buffer[f * numChannels] : buffer[f];
        float inR = (numChannels >= 2) ? buffer[f * numChannels + 1] : buffer[f];

        m_delayBufferL[m_writeIdx] = inL;
        m_delayBufferR[m_writeIdx] = inR;

        // Modulated delay times (LFO)
        float modL = std::sin(m_phaseL * 2.0f * static_cast<float>(M_PI));
        float modR = std::sin(m_phaseR * 2.0f * static_cast<float>(M_PI));

        float delayL = baseDelaySamples + modAmplitude * modL;
        float delayR = baseDelaySamples + modAmplitude * modR;

        // Hermite or linear interpolation
        auto readSample = [bufSize](const std::vector<float>& buf, size_t wIdx, float dSamples) {
            float readPos = static_cast<float>(wIdx) - dSamples;
            while (readPos < 0.0f) readPos += bufSize;
            while (readPos >= bufSize) readPos -= bufSize;

            size_t i0 = static_cast<size_t>(readPos);
            size_t i1 = (i0 + 1) % bufSize;
            float frac = readPos - static_cast<float>(i0);
            return buf[i0] + frac * (buf[i1] - buf[i0]);
        };

        float wetL = readSample(m_delayBufferL, m_writeIdx, delayL);
        float wetR = readSample(m_delayBufferR, m_writeIdx, delayR);

        if (numChannels >= 2) {
            buffer[f * numChannels]     = inL * (1.0f - mix * 0.5f) + wetL * mix;
            buffer[f * numChannels + 1] = inR * (1.0f - mix * 0.5f) + wetR * mix;
        } else {
            buffer[f] = inL * (1.0f - mix * 0.5f) + (wetL + wetR) * 0.5f * mix;
        }

        m_writeIdx = (m_writeIdx + 1) % bufSize;
        m_phaseL += phaseInc;
        if (m_phaseL >= 1.0f) m_phaseL -= 1.0f;
        m_phaseR += phaseInc;
        if (m_phaseR >= 1.0f) m_phaseR -= 1.0f;
    }
}
