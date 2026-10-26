#pragma once

#include <vector>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>

class Chorus {
public:
    Chorus(float sampleRate = 44100.0f);
    void prepare(float sampleRate); // not concurrent with process()
    void reset(); // audio thread only
    void setRate(float rateHz);
    float getRate() const { return m_rate; }

    void setDepth(float depthMs);
    float getDepth() const { return m_depth; }

    void setMix(float mix);
    float getMix() const { return m_mix; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void process(float* buffer, uint32_t numFrames, uint32_t numChannels);

private:
    float m_sampleRate = 44100.0f;
    std::atomic<bool> m_enabled{false};
    std::atomic<float> m_rate{1.2f};
    std::atomic<float> m_depth{3.5f};
    std::atomic<float> m_mix{0.35f};

    std::vector<float> m_delayBufferL;
    std::vector<float> m_delayBufferR;
    size_t m_writeIdx = 0;
    float m_phaseL = 0.0f;
    float m_phaseR = 0.25f; // 90 degree stereo quadrature
};
