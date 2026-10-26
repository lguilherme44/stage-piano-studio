#include "Compressor.hpp"

Compressor::Compressor(float sampleRate) : m_sampleRate(sampleRate) {
    updateCoefficients();
}

void Compressor::setSampleRate(float sampleRate) {
    m_sampleRate = sampleRate;
    m_dirty = true;
}

void Compressor::reset() {
    m_envelopeGrDb = 0.0f;
    m_currentGrDb.store(0.0f);
}

void Compressor::setThreshold(float thresholdDb) {
    m_thresholdDb = std::clamp(thresholdDb, -60.0f, 0.0f);
}

void Compressor::setRatio(float ratio) {
    m_ratio = std::clamp(ratio, 1.0f, 30.0f);
}

void Compressor::setAttack(float attackMs) {
    m_attackMs = std::clamp(attackMs, 0.1f, 200.0f);
    m_dirty = true;
}

void Compressor::setRelease(float releaseMs) {
    m_releaseMs = std::clamp(releaseMs, 5.0f, 2000.0f);
    m_dirty = true;
}

void Compressor::setMakeupGain(float makeupDb) {
    m_makeupDb = std::clamp(makeupDb, -12.0f, 24.0f);
    m_dirty = true;
}

void Compressor::setKnee(float kneeDb) {
    m_kneeDb = std::clamp(kneeDb, 0.0f, 18.0f);
}

void Compressor::updateCoefficients() {
    // Attack/Release 1-pole filter smoothing coefficients
    float attackSec = m_attackMs * 0.001f;
    float releaseSec = m_releaseMs * 0.001f;
    float sr = m_sampleRate;

    m_attackCoeff = std::exp(-1.0f / (attackSec * sr));
    m_releaseCoeff = std::exp(-1.0f / (releaseSec * sr));
    m_makeupLinear = std::pow(10.0f, m_makeupDb / 20.0f);
}

void Compressor::process(float* buffer, uint32_t numFrames, uint32_t numChannels) {
    if (!m_enabled || numChannels == 0) {
        m_currentGrDb.store(0.0f);
        return;
    }

    if (m_dirty.exchange(false, std::memory_order_acq_rel)) {
        updateCoefficients();
    }

    const float thresholdDb = m_thresholdDb;
    const float ratio = m_ratio;
    const float kneeDb = m_kneeDb;
    const float halfKnee = kneeDb * 0.5f;
    const float slope = (ratio > 1.0f) ? (1.0f - 1.0f / ratio) : 0.0f;

    for (uint32_t f = 0; f < numFrames; ++f) {
        float peak = 0.0f;
        if (numChannels >= 2) {
            float absL = std::abs(buffer[f * numChannels]);
            float absR = std::abs(buffer[f * numChannels + 1]);
            peak = std::max(absL, absR);
        } else {
            peak = std::abs(buffer[f]);
        }

        float inDb = 20.0f * std::log10(std::max(peak, 1e-5f));
        float overDb = 0.0f;

        // Soft-knee computation
        float diff = inDb - thresholdDb;
        if (kneeDb > 0.0f) {
            if (diff < -halfKnee) {
                overDb = 0.0f;
            } else if (std::abs(diff) <= halfKnee) {
                overDb = (diff + halfKnee) * (diff + halfKnee) / (2.0f * kneeDb);
            } else {
                overDb = diff;
            }
        } else {
            overDb = std::max(0.0f, diff);
        }

        float targetGrDb = overDb * slope;

        // Smooth gain reduction (attack vs release)
        if (targetGrDb > m_envelopeGrDb) {
            m_envelopeGrDb = m_attackCoeff * m_envelopeGrDb + (1.0f - m_attackCoeff) * targetGrDb;
        } else {
            m_envelopeGrDb = m_releaseCoeff * m_envelopeGrDb + (1.0f - m_releaseCoeff) * targetGrDb;
        }

        float gainLinear = std::pow(10.0f, -m_envelopeGrDb / 20.0f) * m_makeupLinear;

        if (numChannels >= 2) {
            buffer[f * numChannels] *= gainLinear;
            buffer[f * numChannels + 1] *= gainLinear;
        } else {
            buffer[f] *= gainLinear;
        }
    }

    m_currentGrDb.store(m_envelopeGrDb);
}
