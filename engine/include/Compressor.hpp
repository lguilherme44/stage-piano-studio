#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <atomic>

class Compressor {
public:
    Compressor(float sampleRate = 44100.0f);

    void setSampleRate(float sampleRate);
    void reset();

    void setThreshold(float thresholdDb);
    float getThreshold() const { return m_thresholdDb; }

    void setRatio(float ratio);
    float getRatio() const { return m_ratio; }

    void setAttack(float attackMs);
    float getAttack() const { return m_attackMs; }

    void setRelease(float releaseMs);
    float getRelease() const { return m_releaseMs; }

    void setMakeupGain(float makeupDb);
    float getMakeupGain() const { return m_makeupDb; }

    void setKnee(float kneeDb);
    float getKnee() const { return m_kneeDb; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    float getGainReductionDb() const { return m_currentGrDb.load(); }

    void process(float* buffer, uint32_t numFrames, uint32_t numChannels);

private:
    void updateCoefficients();

    float m_sampleRate = 44100.0f;
    bool m_enabled = true;

    float m_thresholdDb = -12.0f;
    float m_ratio = 3.0f;
    float m_attackMs = 15.0f;
    float m_releaseMs = 150.0f;
    float m_makeupDb = 2.0f;
    float m_kneeDb = 4.0f;

    float m_attackCoeff = 0.99f;
    float m_releaseCoeff = 0.999f;
    float m_makeupLinear = 1.2589f;

    float m_envelopeGrDb = 0.0f;
    std::atomic<float> m_currentGrDb{0.0f};

    std::mutex m_paramMutex;
};
