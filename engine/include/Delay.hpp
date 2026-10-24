#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>

class Delay {
public:
    Delay(float sampleRate = 44100.0f);
    void reset();
    void setTime(float timeMs);
    float getTime() const { return m_timeMs; }

    void setFeedback(float fb);
    float getFeedback() const { return m_feedback; }

    void setMix(float mix);
    float getMix() const { return m_mix; }

    void setPingPong(bool pp) { m_pingPong = pp; }
    bool isPingPong() const { return m_pingPong; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void process(float* buffer, uint32_t numFrames, uint32_t numChannels);

private:
    float m_sampleRate = 44100.0f;
    bool m_enabled = false;
    bool m_pingPong = true;
    float m_timeMs = 350.0f;
    float m_feedback = 0.38f;
    float m_mix = 0.25f;

    std::vector<float> m_bufferL;
    std::vector<float> m_bufferR;
    size_t m_writeIdx = 0;
    float m_dampL = 0.0f;
    float m_dampR = 0.0f;
};
