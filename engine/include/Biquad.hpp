#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum class FilterType {
    Lowpass,
    Highpass,
    Bandpass,
    LowShelf,
    Peaking,
    HighShelf
};

class Biquad {
public:
    Biquad() {
        reset();
    }

    void reset() {
        m_s1 = 0.0f;
        m_s2 = 0.0f;
    }

    void configure(FilterType type, float sampleRate, float frequency, float qOrSlope, float gainDb = 0.0f) {
        if (sampleRate <= 0.0f) sampleRate = 44100.0f;
        frequency = std::clamp(frequency, 10.0f, sampleRate * 0.49f);
        qOrSlope = std::max(0.05f, qOrSlope);

        const float omega = 2.0f * static_cast<float>(M_PI) * frequency / sampleRate;
        const float cosOmega = std::cos(omega);
        const float sinOmega = std::sin(omega);

        float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;

        switch (type) {
            case FilterType::Lowpass: {
                const float alpha = sinOmega / (2.0f * qOrSlope);
                b0 = (1.0f - cosOmega) * 0.5f;
                b1 = 1.0f - cosOmega;
                b2 = (1.0f - cosOmega) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosOmega;
                a2 = 1.0f - alpha;
                break;
            }
            case FilterType::Highpass: {
                const float alpha = sinOmega / (2.0f * qOrSlope);
                b0 = (1.0f + cosOmega) * 0.5f;
                b1 = -(1.0f + cosOmega);
                b2 = (1.0f + cosOmega) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosOmega;
                a2 = 1.0f - alpha;
                break;
            }
            case FilterType::Bandpass: {
                const float alpha = sinOmega / (2.0f * qOrSlope);
                b0 = alpha;
                b1 = 0.0f;
                b2 = -alpha;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosOmega;
                a2 = 1.0f - alpha;
                break;
            }
            case FilterType::Peaking: {
                const float A = std::pow(10.0f, gainDb / 40.0f);
                const float alpha = sinOmega / (2.0f * qOrSlope);
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cosOmega;
                b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A;
                a1 = -2.0f * cosOmega;
                a2 = 1.0f - alpha / A;
                break;
            }
            case FilterType::LowShelf: {
                const float A = std::pow(10.0f, gainDb / 40.0f);
                const float beta = std::sqrt(2.0f * A) * sinOmega;
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega + beta);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosOmega);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega - beta);
                a0 = (A + 1.0f) + (A - 1.0f) * cosOmega + beta;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosOmega);
                a2 = (A + 1.0f) + (A - 1.0f) * cosOmega - beta;
                break;
            }
            case FilterType::HighShelf: {
                const float A = std::pow(10.0f, gainDb / 40.0f);
                const float beta = std::sqrt(2.0f * A) * sinOmega;
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega + beta);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosOmega);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega - beta);
                a0 = (A + 1.0f) - (A - 1.0f) * cosOmega + beta;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosOmega);
                a2 = (A + 1.0f) - (A - 1.0f) * cosOmega - beta;
                break;
            }
        }

        const float invA0 = 1.0f / a0;
        m_b0 = b0 * invA0;
        m_b1 = b1 * invA0;
        m_b2 = b2 * invA0;
        m_a1 = a1 * invA0;
        m_a2 = a2 * invA0;
    }

    // Direct Form II Transposed: single sample processing
    inline float process(float in) {
        float out = m_b0 * in + m_s1;
        m_s1 = m_b1 * in - m_a1 * out + m_s2;
        m_s2 = m_b2 * in - m_a2 * out;
        return out;
    }

private:
    float m_b0 = 1.0f, m_b1 = 0.0f, m_b2 = 0.0f;
    float m_a1 = 0.0f, m_a2 = 0.0f;
    float m_s1 = 0.0f, m_s2 = 0.0f;
};
