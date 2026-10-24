#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>

class Reverb {
public:
    Reverb(float sampleRate = 44100.0f);
    void reset();

    void setDecay(float decay); // 0.1 to 0.98
    float getDecay() const { return m_decay; }

    void setDamping(float damp); // 0.0 to 0.8
    float getDamping() const { return m_damp; }

    void setMix(float mix); // 0.0 to 1.0
    float getMix() const { return m_mix; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void process(float* buffer, uint32_t numFrames, uint32_t numChannels);

private:
    struct Comb {
        std::vector<float> buf;
        size_t idx = 0;
        float store = 0.0f;
        float process(float in, float feedback, float damp) {
            float out = buf[idx];
            store = out * (1.0f - damp) + store * damp;
            buf[idx] = in + store * feedback;
            idx = (idx + 1) % buf.size();
            return out;
        }
    };

    struct AllPass {
        std::vector<float> buf;
        size_t idx = 0;
        float process(float in) {
            float bufOut = buf[idx];
            float out = -in + bufOut;
            buf[idx] = in + (bufOut * 0.5f);
            idx = (idx + 1) % buf.size();
            return out;
        }
    };

    float m_sampleRate = 44100.0f;
    bool m_enabled = true;
    float m_decay = 0.72f;
    float m_damp = 0.35f;
    float m_mix = 0.28f;

    Comb m_combsL[4];
    Comb m_combsR[4];
    AllPass m_allPassL[2];
    AllPass m_allPassR[2];
};
