#include "Delay.hpp"

Delay::Delay(float sampleRate) {
    prepare(sampleRate);
}

void Delay::prepare(float sampleRate) {
    m_sampleRate = sampleRate;
    size_t maxDelay = static_cast<size_t>(m_sampleRate * 2.0f); // 2 seconds max
    m_bufferL.assign(maxDelay, 0.0f);
    m_bufferR.assign(maxDelay, 0.0f);
    m_writeIdx = 0;
}

void Delay::reset() {
    std::fill(m_bufferL.begin(), m_bufferL.end(), 0.0f);
    std::fill(m_bufferR.begin(), m_bufferR.end(), 0.0f);
    m_writeIdx = 0;
    m_dampL = 0.0f;
    m_dampR = 0.0f;
}

void Delay::setTime(float timeMs) {
    m_timeMs = std::clamp(timeMs, 10.0f, 1500.0f);
}

void Delay::setFeedback(float fb) {
    m_feedback = std::clamp(fb, 0.0f, 0.85f);
}

void Delay::setMix(float mix) {
    m_mix = std::clamp(mix, 0.0f, 1.0f);
}

void Delay::process(float* buffer, uint32_t numFrames, uint32_t numChannels) {
    if (!m_enabled || m_mix <= 0.001f || numChannels == 0) return;

    const float mix = m_mix;
    const float feedback = m_feedback;
    const bool pingPong = m_pingPong;
    size_t bufSize = m_bufferL.size();
    size_t delaySamples = static_cast<size_t>(m_timeMs * 0.001f * m_sampleRate);
    if (delaySamples >= bufSize) delaySamples = bufSize - 1;

    for (uint32_t f = 0; f < numFrames; ++f) {
        float inL = (numChannels >= 2) ? buffer[f * numChannels] : buffer[f];
        float inR = (numChannels >= 2) ? buffer[f * numChannels + 1] : buffer[f];

        size_t readIdx = (m_writeIdx + bufSize - delaySamples) % bufSize;
        float delayedL = m_bufferL[readIdx];
        float delayedR = m_bufferR[readIdx];

        // 1-pole damping filter
        m_dampL = 0.6f * delayedL + 0.4f * m_dampL;
        m_dampR = 0.6f * delayedR + 0.4f * m_dampR;

        // Ping-pong cross feedback or stereo feedback
        if (pingPong) {
            m_bufferL[m_writeIdx] = inL + m_dampR * feedback;
            m_bufferR[m_writeIdx] = inR + m_dampL * feedback;
        } else {
            m_bufferL[m_writeIdx] = inL + m_dampL * feedback;
            m_bufferR[m_writeIdx] = inR + m_dampR * feedback;
        }

        if (numChannels >= 2) {
            buffer[f * numChannels]     = inL + delayedL * mix;
            buffer[f * numChannels + 1] = inR + delayedR * mix;
        } else {
            buffer[f] = inL + (delayedL + delayedR) * 0.5f * mix;
        }

        m_writeIdx = (m_writeIdx + 1) % bufSize;
    }
}
