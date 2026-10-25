#include "Sampler.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstring>

#pragma pack(push, 1)
struct WavHeaderView {
    char riff[4];
    uint32_t riff_size;
    char wave[4];
};

struct ChunkHeader {
    char id[4];
    uint32_t size;
};

struct FmtChunk {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};
#pragma pack(pop)

Sampler::Sampler(size_t maxPolyphony, float sampleRate)
    : m_sampleRate(sampleRate),
      m_maxPolyphony(maxPolyphony),
      m_equalizer(sampleRate),
      m_compressor(sampleRate) {
    m_voices.resize(m_maxPolyphony);
    applyPreset(m_preset);
}

Sampler::~Sampler() {
    allNotesOff();
}

void Sampler::applyPreset(const TimbrePreset& preset) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset = preset;

    // Apply EQ parameters
    m_equalizer.setEnabled(preset.eqEnabled);
    m_equalizer.setLowGain(preset.eqLowGainDb);
    m_equalizer.setLowFrequency(preset.eqLowFreqHz);
    m_equalizer.setMidGain(preset.eqMidGainDb);
    m_equalizer.setMidFrequency(preset.eqMidFreqHz);
    m_equalizer.setMidQ(preset.eqMidQ);
    m_equalizer.setHighGain(preset.eqHighGainDb);
    m_equalizer.setHighFrequency(preset.eqHighFreqHz);

    // Apply Compressor parameters
    m_compressor.setEnabled(preset.compEnabled);
    m_compressor.setThreshold(preset.compThresholdDb);
    m_compressor.setRatio(preset.compRatio);
    m_compressor.setAttack(preset.compAttackMs);
    m_compressor.setRelease(preset.compReleaseMs);
    m_compressor.setMakeupGain(preset.compMakeupDb);
    m_compressor.setKnee(preset.compKneeDb);
}

TimbrePreset Sampler::getCurrentPreset() const {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    return m_preset;
}

bool Sampler::savePreset(const std::string& filepath) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.baseInstrument = m_instrumentName;
    return m_preset.saveToFile(filepath);
}

bool Sampler::loadPreset(const std::string& filepath) {
    TimbrePreset p;
    if (!p.loadFromFile(filepath)) {
        return false;
    }
    applyPreset(p);
    return true;
}

void Sampler::setFilterCutoff(float hz) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.filterCutoff = std::clamp(hz, 20.0f, 20000.0f);
}

void Sampler::setFilterResonance(float res) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.filterResonance = std::clamp(res, 0.0f, 0.95f);
}

void Sampler::setAttackMs(float ms) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.attackMs = std::clamp(ms, 0.0f, 5000.0f);
}

void Sampler::setReleaseMs(float ms) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.releaseMs = std::clamp(ms, 20.0f, 5000.0f);
}

void Sampler::setLowGain(float dB) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.eqLowGainDb = dB;
    m_equalizer.setLowGain(dB);
}

void Sampler::setMidGain(float dB) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.eqMidGainDb = dB;
    m_equalizer.setMidGain(dB);
}

void Sampler::setHighGain(float dB) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.eqHighGainDb = dB;
    m_equalizer.setHighGain(dB);
}

void Sampler::setCompressorEnabled(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.compEnabled = enabled;
    m_compressor.setEnabled(enabled);
}

void Sampler::setCompressorThreshold(float dB) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.compThresholdDb = dB;
    m_compressor.setThreshold(dB);
}

void Sampler::setCompressorRatio(float ratio) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.compRatio = ratio;
    m_compressor.setRatio(ratio);
}

void Sampler::setCompressorMakeup(float dB) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.compMakeupDb = dB;
    m_compressor.setMakeupGain(dB);
}

void Sampler::setMasterVolume(float vol) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_preset.masterVolume = std::clamp(vol, 0.0f, 2.0f);
}

std::shared_ptr<SampleData> Sampler::getOrLoadSample(const std::string& filename) {
    auto it = m_sampleCache.find(filename);
    if (it != m_sampleCache.end()) {
        return it->second;
    }

    std::string filepath = m_samplesDir + "/" + filename;
    std::ifstream in(filepath, std::ios::binary);
    if (!in) {
        return nullptr;
    }

    WavHeaderView wv;
    if (!in.read(reinterpret_cast<char*>(&wv), sizeof(wv))) return nullptr;
    if (memcmp(wv.riff, "RIFF", 4) != 0 || memcmp(wv.wave, "WAVE", 4) != 0) return nullptr;

    FmtChunk fmt{};
    bool hasFmt = false;
    std::vector<int16_t> pcmData;

    while (in) {
        ChunkHeader ch;
        if (!in.read(reinterpret_cast<char*>(&ch), sizeof(ch))) break;

        if (memcmp(ch.id, "fmt ", 4) == 0) {
            in.read(reinterpret_cast<char*>(&fmt), std::min<size_t>(ch.size, sizeof(fmt)));
            if (ch.size > sizeof(fmt)) in.seekg(ch.size - sizeof(fmt), std::ios::cur);
            hasFmt = true;
        } else if (memcmp(ch.id, "data", 4) == 0) {
            pcmData.resize(ch.size / sizeof(int16_t));
            in.read(reinterpret_cast<char*>(pcmData.data()), ch.size);
            break;
        } else {
            in.seekg(ch.size, std::ios::cur);
        }
    }

    if (!hasFmt || pcmData.empty()) return nullptr;

    auto sample = std::make_shared<SampleData>();
    sample->filename = filename;
    sample->sampleRate = fmt.sample_rate;
    sample->channels = fmt.num_channels;
    sample->numFrames = pcmData.size() / fmt.num_channels;
    sample->pcm = std::move(pcmData);

    m_sampleCache[filename] = sample;
    return sample;
}

bool Sampler::loadInstrument(const std::string& jsonPath, const std::string& samplesDir) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    allNotesOff();
    m_zones.clear();
    m_samplesDir = samplesDir;

    std::ifstream in(jsonPath);
    if (!in) {
        std::cerr << "[Sampler] Error: Cannot open instrument definition: " << jsonPath << std::endl;
        return false;
    }

    std::string line;
    int currentNote = -1;
    m_instrumentName = "Stage Piano";

    while (std::getline(in, line)) {
        if (line.find("\"name\":") != std::string::npos) {
            size_t start = line.find('"', line.find(':')) + 1;
            size_t end = line.rfind('"');
            if (start < end) m_instrumentName = line.substr(start, end - start);
        } else if (line.find("\"") != std::string::npos && line.find("\": [") != std::string::npos) {
            size_t q1 = line.find('"');
            size_t q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                currentNote = std::stoi(line.substr(q1 + 1, q2 - q1 - 1));
                m_zones[currentNote].noteNumber = currentNote;
            }
        } else if (line.find("\"min_velocity\":") != std::string::npos && currentNote != -1) {
            VelocityLayer layer{};
            layer.minVelocity = std::stoi(line.substr(line.find(':') + 1));
            
            std::getline(in, line);
            layer.maxVelocity = std::stoi(line.substr(line.find(':') + 1));
            
            std::getline(in, line);
            layer.targetVelocity = std::stoi(line.substr(line.find(':') + 1));
            
            std::getline(in, line);
            size_t f1 = line.find('"', line.find(':')) + 1;
            size_t f2 = line.rfind('"');
            layer.filename = line.substr(f1, f2 - f1);
            layer.sample = nullptr;
            
            m_zones[currentNote].layers.push_back(layer);
        }
    }

    m_preset.baseInstrument = m_instrumentName;

    std::cout << "[Sampler] Loaded instrument '" << m_instrumentName << "' ("
              << m_zones.size() << " note zones configured, instant lazy loading active)" << std::endl;
    return true;
}

const VelocityLayer* Sampler::findBestLayer(int note, int velocity, int& outRootNote) {
    if (m_zones.empty()) return nullptr;

    auto it = m_zones.find(note);
    if (it == m_zones.end()) {
        int bestDist = 1000;
        int bestNote = -1;
        for (const auto& [n, zone] : m_zones) {
            int d = std::abs(n - note);
            if (d < bestDist) {
                bestDist = d;
                bestNote = n;
            }
        }
        if (bestNote == -1) return nullptr;
        it = m_zones.find(bestNote);
    }

    outRootNote = it->first;
    const auto& layers = it->second.layers;
    if (layers.empty()) return nullptr;

    for (const auto& layer : layers) {
        if (velocity >= layer.minVelocity && velocity <= layer.maxVelocity) {
            return &layer;
        }
    }

    return &layers.back();
}

void Sampler::noteOn(int note, int velocity, bool looping) {
    if (velocity <= 0) {
        noteOff(note);
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    
    int rootNote = note;
    const VelocityLayer* layer = findBestLayer(note, velocity, rootNote);
    if (!layer) return;

    if (!layer->sample) {
        layer->sample = getOrLoadSample(layer->filename);
    }
    if (!layer->sample) return;

    // Retrigger check: if the same note is already sounding, fade it out to prevent phase stacking & mud
    for (auto& oldV : m_voices) {
        if (oldV.active && oldV.note == note) {
            oldV.stage = Voice::Release;
            oldV.releaseStep = std::max(oldV.releaseStep, 1.0f / (m_sampleRate * 0.020f)); // 20ms smooth anti-mud release
        }
    }

    // Find voice
    int voiceIdx = -1;
    uint64_t oldestAge = UINT64_MAX;
    int oldestIdx = 0;

    for (size_t i = 0; i < m_voices.size(); ++i) {
        if (!m_voices[i].active) {
            voiceIdx = static_cast<int>(i);
            break;
        }
        if (m_voices[i].age < oldestAge) {
            oldestAge = m_voices[i].age;
            oldestIdx = static_cast<int>(i);
        }
    }

    if (voiceIdx == -1) {
        voiceIdx = oldestIdx;
    }

    Voice& v = m_voices[voiceIdx];
    v.active = true;
    v.isLooping = looping;
    v.note = note;
    v.velocity = velocity;
    v.sample = layer->sample;
    v.position = 0.0;
    
    v.playbackRate = std::pow(2.0, (note - rootNote) / 12.0);
    float normVel = static_cast<float>(velocity) / 127.0f;
    v.gain = std::pow(normVel, 1.3f);

    // Envelope configuration
    if (m_preset.attackMs <= 2.0f) {
        v.stage = Voice::Decay;
        v.envLevel = 1.0f;
    } else {
        v.stage = Voice::Attack;
        v.envLevel = 0.0f;
        v.attackStep = 1.0f / (m_preset.attackMs * 0.001f * m_sampleRate);
    }

    float decaySamples = std::max(10.0f, m_preset.decayMs * 0.001f * m_sampleRate);
    v.sustainLevel = m_preset.sustainLevel;
    v.decayStep = (1.0f - v.sustainLevel) / decaySamples;

    float releaseSamples = std::max(10.0f, m_preset.releaseMs * 0.001f * m_sampleRate);
    v.releaseStep = 1.0f / releaseSamples;

    v.sustained = false;
    v.age = ++m_voiceAgeCounter;

    // Configure Per-Voice Resonant Cutoff Filter
    float q = 0.707f / std::max(0.05f, (1.0f - m_preset.filterResonance * 0.95f));
    v.filter[0].configure(FilterType::Lowpass, m_sampleRate, m_preset.filterCutoff, q);
    v.filter[1].configure(FilterType::Lowpass, m_sampleRate, m_preset.filterCutoff, q);
    v.filter[0].reset();
    v.filter[1].reset();
}

void Sampler::noteOff(int note) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    for (auto& v : m_voices) {
        if (v.active && v.note == note) {
            if (m_sustainPedal) {
                v.sustained = true;
            } else {
                v.stage = Voice::Release;
            }
        }
    }
}

void Sampler::setSustainPedal(bool pressed) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    m_sustainPedal = pressed;
    if (!pressed) {
        for (auto& v : m_voices) {
            if (v.active && v.sustained) {
                v.sustained = false;
                v.stage = Voice::Release;
            }
        }
    }
}

void Sampler::allNotesOff() {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    for (auto& v : m_voices) {
        v.active = false;
        v.sample = nullptr;
    }
    m_activeVoices = 0;
}

int Sampler::getActiveVoiceCount() const {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);
    int count = 0;
    for (const auto& v : m_voices) {
        if (v.active) count++;
    }
    return count;
}

void Sampler::render(float* outputBuffer, uint32_t numFrames, uint32_t numChannels) {
    std::lock_guard<std::recursive_mutex> lock(m_samplerMutex);

    int activeCount = 0;
    const float inv32768 = 1.0f / 32768.0f;
    const bool useFilter = (m_preset.filterCutoff < 19000.0f || m_preset.filterResonance > 0.05f);

    for (auto& v : m_voices) {
        if (!v.active || !v.sample) continue;

        activeCount++;
        const int16_t* pcm = v.sample->pcm.data();
        size_t sampleFrames = v.sample->numFrames;
        uint16_t sampleCh = v.sample->channels;

        for (uint32_t f = 0; f < numFrames; ++f) {
            if (!v.isLooping && v.position >= sampleFrames - 1) {
                v.active = false;
                break;
            }

            // ADSR Envelope handling
            switch (v.stage) {
                case Voice::Attack:
                    v.envLevel += v.attackStep;
                    if (v.envLevel >= 1.0f) {
                        v.envLevel = 1.0f;
                        v.stage = Voice::Decay;
                    }
                    break;
                case Voice::Decay:
                    if (v.envLevel > v.sustainLevel) {
                        v.envLevel -= v.decayStep;
                        if (v.envLevel < v.sustainLevel) v.envLevel = v.sustainLevel;
                    } else {
                        v.stage = Voice::Sustain;
                    }
                    break;
                case Voice::Sustain:
                    v.envLevel = v.sustainLevel;
                    break;
                case Voice::Release:
                    v.envLevel -= v.releaseStep;
                    if (v.envLevel <= 0.0f) {
                        v.envLevel = 0.0f;
                        v.active = false;
                        break;
                    }
                    break;
                case Voice::Off:
                case Voice::Idle:
                    v.active = false;
                    break;
            }

            if (!v.active) break;

            size_t idx0 = static_cast<size_t>(v.position);
            size_t idx1 = std::min(idx0 + 1, sampleFrames - 1);
            float frac = static_cast<float>(v.position - idx0);

            float sL = 0.0f, sR = 0.0f;
            if (sampleCh == 2) {
                float s0_L = static_cast<float>(pcm[idx0 * 2]) * inv32768;
                float s1_L = static_cast<float>(pcm[idx1 * 2]) * inv32768;
                float s0_R = static_cast<float>(pcm[idx0 * 2 + 1]) * inv32768;
                float s1_R = static_cast<float>(pcm[idx1 * 2 + 1]) * inv32768;

                sL = s0_L + frac * (s1_L - s0_L);
                sR = s0_R + frac * (s1_R - s0_R);
            } else {
                float s0 = static_cast<float>(pcm[idx0]) * inv32768;
                float s1 = static_cast<float>(pcm[idx1]) * inv32768;
                sL = sR = s0 + frac * (s1 - s0);
            }

            // Continuous seamless crossfade loop for ambient drones
            if (v.isLooping && sampleFrames > 88200) {
                size_t loopStart = static_cast<size_t>(sampleFrames * 0.15);
                size_t loopEnd = static_cast<size_t>(sampleFrames * 0.85);
                size_t xfadeLen = std::min<size_t>(static_cast<size_t>(m_sampleRate * 2.0f), (loopEnd - loopStart) / 3);

                if (v.position >= (loopEnd - xfadeLen)) {
                    double xfadePos = v.position - (loopEnd - xfadeLen);
                    double progress = std::clamp(xfadePos / static_cast<double>(xfadeLen), 0.0, 1.0);
                    float gainA = std::cos(static_cast<float>(progress * 1.5707963267948966));
                    float gainB = std::sin(static_cast<float>(progress * 1.5707963267948966));

                    double bPos = loopStart + xfadePos;
                    size_t bIdx0 = static_cast<size_t>(bPos) % sampleFrames;
                    size_t bIdx1 = std::min(bIdx0 + 1, sampleFrames - 1);
                    float bFrac = static_cast<float>(bPos - static_cast<size_t>(bPos));

                    float bL = 0.0f, bR = 0.0f;
                    if (sampleCh == 2) {
                        float b0_L = static_cast<float>(pcm[bIdx0 * 2]) * inv32768;
                        float b1_L = static_cast<float>(pcm[bIdx1 * 2]) * inv32768;
                        float b0_R = static_cast<float>(pcm[bIdx0 * 2 + 1]) * inv32768;
                        float b1_R = static_cast<float>(pcm[bIdx1 * 2 + 1]) * inv32768;
                        bL = b0_L + bFrac * (b1_L - b0_L);
                        bR = b0_R + bFrac * (b1_R - b0_R);
                    } else {
                        float b0 = static_cast<float>(pcm[bIdx0]) * inv32768;
                        float b1 = static_cast<float>(pcm[bIdx1]) * inv32768;
                        bL = bR = b0 + bFrac * (b1 - b0);
                    }

                    sL = sL * gainA + bL * gainB;
                    sR = sR * gainA + bR * gainB;

                    if (v.position >= loopEnd) {
                        v.position = loopStart + (v.position - loopEnd);
                    }
                }
            }

            // Apply Voice Filter (Tone/Brightness/Resonance)
            if (useFilter) {
                sL = v.filter[0].process(sL);
                sR = v.filter[1].process(sR);
            }

            float finalGain = v.gain * v.envLevel * m_preset.masterVolume;
            sL *= finalGain;
            sR *= finalGain;

            if (numChannels >= 2) {
                outputBuffer[f * numChannels] += sL;
                outputBuffer[f * numChannels + 1] += sR;
            } else {
                outputBuffer[f] += (sL + sR) * 0.5f;
            }

            v.position += v.playbackRate;
        }
    }

    m_activeVoices = activeCount;

    // 1. Apply Studio 3-Band Equalizer (Grave, Médio, Agudo)
    m_equalizer.process(outputBuffer, numFrames, numChannels);

    // 2. Apply Dynamic Compressor (Peak/RMS, Soft-Knee, Makeup)
    m_compressor.process(outputBuffer, numFrames, numChannels);

    // 3. Studio Master Limiter / Soft-Saturation (prevents harsh DAC digital clipping)
    const uint32_t totalSamples = numFrames * numChannels;
    for (uint32_t i = 0; i < totalSamples; ++i) {
        float s = outputBuffer[i];
        if (s > 1.0f) {
            outputBuffer[i] = 1.0f - std::exp(-(s - 1.0f) * 0.5f);
        } else if (s < -1.0f) {
            outputBuffer[i] = -1.0f + std::exp((s + 1.0f) * 0.5f);
        }
    }
}
