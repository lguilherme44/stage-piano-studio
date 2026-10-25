#pragma once

#include "Equalizer.hpp"
#include "Compressor.hpp"
#include "TimbrePreset.hpp"
#include "Biquad.hpp"
#include "DiskStreamer.hpp"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>

constexpr size_t DFD_PRELOAD_FRAMES = 32768; // ~0.74 seconds preload in RAM
constexpr size_t DFD_CHUNK_FRAMES = 16384;   // ~0.37 seconds per disk streaming chunk

struct SampleData {
    std::string filename;
    std::string fullPath;
    uint32_t sampleRate = 44100;
    uint16_t channels = 2;
    size_t numFrames = 0;
    size_t dataByteOffset = 0;

    // RAM Preload: first DFD_PRELOAD_FRAMES frames of the sample
    std::vector<int16_t> pcm;
    size_t preloadFrames = 0;
    bool isFullyPreloaded = false;
};

struct VelocityLayer {
    int minVelocity;
    int maxVelocity;
    int targetVelocity;
    std::string filename;
    mutable std::shared_ptr<SampleData> sample;
};

struct NoteZone {
    int noteNumber;
    std::vector<VelocityLayer> layers;
};

struct Voice {
    Voice() : ringBuffer(std::make_unique<VoiceRingBuffer>(65536, 2)) {}
    Voice(Voice&& other) noexcept = default;
    Voice& operator=(Voice&& other) noexcept = default;
    Voice(const Voice&) = delete;
    Voice& operator=(const Voice&) = delete;

    bool active = false;
    bool isLooping = false;
    int note = -1;
    int velocity = 0;
    std::shared_ptr<SampleData> sample = nullptr;
    double position = 0.0;
    double playbackRate = 1.0;
    float gain = 1.0f;
    
    enum Stage { Idle, Attack, Decay, Sustain, Release, Off };
    Stage stage = Off;
    float envLevel = 0.0f;
    float attackStep = 0.005f;
    float decayStep = 0.0005f;
    float sustainLevel = 1.0f;
    float releaseStep = 0.0006f;
    bool sustained = false;
    uint64_t age = 0;

    // Per-voice stereo filter (for cutoff & resonance timbre shaping)
    Biquad filter[2];

    // DFD Streaming Components
    uint32_t voiceId = 0;
    std::unique_ptr<VoiceRingBuffer> ringBuffer;
    std::shared_ptr<std::atomic<bool>> cancelToken;
    std::shared_ptr<std::atomic<bool>> inFlightRequest;
    size_t nextDiskFrame = 0;
    size_t ringBufferHeadFrame = 0;
};

class Sampler {
public:
    Sampler(size_t maxPolyphony = 128, float sampleRate = 44100.0f);
    ~Sampler();

    bool loadInstrument(const std::string& jsonPath, const std::string& samplesDir);
    
    void noteOn(int note, int velocity, bool looping = false);
    void noteOff(int note);
    void setSustainPedal(bool pressed);
    void allNotesOff();

    void render(float* outputBuffer, uint32_t numFrames, uint32_t numChannels);

    int getActiveVoiceCount() const;
    std::string getInstrumentName() const { return m_instrumentName; }
    void setMasterVolume(float vol);
    float getMasterVolume() const { return m_preset.masterVolume; }

    std::shared_ptr<SampleData> getOrLoadSample(const std::string& filename);

    // Timbre & Effects Control
    void applyPreset(const TimbrePreset& preset);
    TimbrePreset getCurrentPreset() const;
    bool savePreset(const std::string& filepath);
    bool loadPreset(const std::string& filepath);

    // Live DSP Adjustments
    void setFilterCutoff(float hz);
    void setFilterResonance(float res);
    void setAttackMs(float ms);
    void setReleaseMs(float ms);

    void setLowGain(float dB);
    void setMidGain(float dB);
    void setHighGain(float dB);

    void setCompressorEnabled(bool enabled);
    void setCompressorThreshold(float dB);
    void setCompressorRatio(float ratio);
    void setCompressorMakeup(float dB);

    Equalizer& getEqualizer() { return m_equalizer; }
    Compressor& getCompressor() { return m_compressor; }

private:
    const VelocityLayer* findBestLayer(int note, int velocity, int& outRootNote);

    std::string m_instrumentName = "None";
    std::string m_samplesDir = "";
    float m_sampleRate = 44100.0f;

    std::map<int, NoteZone> m_zones;
    std::map<std::string, std::shared_ptr<SampleData>> m_sampleCache;
    
    std::vector<Voice> m_voices;
    size_t m_maxPolyphony;
    std::atomic<int> m_activeVoices{0};
    
    bool m_sustainPedal = false;
    uint64_t m_voiceAgeCounter = 0;

    // Timbre, EQ & Dynamics Engines
    TimbrePreset m_preset;
    Equalizer m_equalizer;
    Compressor m_compressor;
    
    mutable std::recursive_mutex m_samplerMutex;
};
