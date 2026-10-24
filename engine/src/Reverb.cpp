#include "Reverb.hpp"

Reverb::Reverb(float sampleRate) : m_sampleRate(sampleRate) {
    // Prime-length comb and allpass delay buffers (scaled to sample rate)
    const int combLengthsL[4] = { 1116, 1188, 1277, 1356 };
    const int combLengthsR[4] = { 1139, 1211, 1300, 1379 };
    const int allPassLengthsL[2] = { 556, 441 };
    const int allPassLengthsR[2] = { 579, 464 };

    float scale = m_sampleRate / 44100.0f;

    for (int i = 0; i < 4; ++i) {
        m_combsL[i].buf.resize(static_cast<size_t>(combLengthsL[i] * scale), 0.0f);
        m_combsR[i].buf.resize(static_cast<size_t>(combLengthsR[i] * scale), 0.0f);
    }

    for (int i = 0; i < 2; ++i) {
        m_allPassL[i].buf.resize(static_cast<size_t>(allPassLengthsL[i] * scale), 0.0f);
        m_allPassR[i].buf.resize(static_cast<size_t>(allPassLengthsR[i] * scale), 0.0f);
    }
}

void Reverb::reset() {
    for (int i = 0; i < 4; ++i) {
        std::fill(m_combsL[i].buf.begin(), m_combsL[i].buf.end(), 0.0f);
        std::fill(m_combsR[i].buf.begin(), m_combsR[i].buf.end(), 0.0f);
        m_combsL[i].store = 0.0f;
        m_combsR[i].store = 0.0f;
    }
    for (int i = 0; i < 2; ++i) {
        std::fill(m_allPassL[i].buf.begin(), m_allPassL[i].buf.end(), 0.0f);
        std::fill(m_allPassR[i].buf.begin(), m_allPassR[i].buf.end(), 0.0f);
    }
}

void Reverb::setDecay(float decay) {
    m_decay = std::clamp(decay, 0.1f, 0.98f);
}

void Reverb::setDamping(float damp) {
    m_damp = std::clamp(damp, 0.0f, 0.85f);
}

void Reverb::setMix(float mix) {
    m_mix = std::clamp(mix, 0.0f, 1.0f);
}

void Reverb::process(float* buffer, uint32_t numFrames, uint32_t numChannels) {
    if (!m_enabled || m_mix <= 0.001f || numChannels == 0) return;

    for (uint32_t f = 0; f < numFrames; ++f) {
        float inL = (numChannels >= 2) ? buffer[f * numChannels] : buffer[f];
        float inR = (numChannels >= 2) ? buffer[f * numChannels + 1] : buffer[f];
        float inputMono = (inL + inR) * 0.015f; // Attenuate input to prevent comb saturation

        // Parallel Comb Filters
        float outCombL = 0.0f, outCombR = 0.0f;
        for (int i = 0; i < 4; ++i) {
            outCombL += m_combsL[i].process(inputMono, m_decay, m_damp);
            outCombR += m_combsR[i].process(inputMono, m_decay, m_damp);
        }

        // Series All-Pass Diffusers
        for (int i = 0; i < 2; ++i) {
            outCombL = m_allPassL[i].process(outCombL);
            outCombR = m_allPassR[i].process(outCombR);
        }

        if (numChannels >= 2) {
            buffer[f * numChannels]     = inL + outCombL * m_mix;
            buffer[f * numChannels + 1] = inR + outCombR * m_mix;
        } else {
            buffer[f] = inL + (outCombL + outCombR) * 0.5f * m_mix;
        }
    }
}
