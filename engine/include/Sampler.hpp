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

    // RAM Preload: first DFD_PRELOAD_FRAMES frames of the sample (or all of it)
    std::vector<int16_t> pcm;
    size_t preloadFrames = 0;
    bool isFullyPreloaded = false;
};

struct VelocityLayer {
    int minVelocity;
    int maxVelocity;
    int targetVelocity;
    std::string filename;
    std::shared_ptr<SampleData> sample;
};

struct NoteZone {
    int noteNumber;
    std::vector<VelocityLayer> layers;
};

// Immutable once published. Built and preloaded off the audio thread, swapped in by pointer,
// and freed by the I/O thread once nothing (current slot, pending slot, voices) references it.
struct InstrumentData {
    std::string name;
    std::map<int, NoteZone> zones;
    std::map<std::string, std::shared_ptr<SampleData>> sampleCache;
    std::atomic<int> refs{0};
};

struct Voice {
    Voice() : ringBuffer(std::make_unique<VoiceRingBuffer>(65536, 2)) {}
    Voice(Voice&& other) noexcept = default;
    Voice& operator=(Voice&& other) noexcept = default;
    Voice(const Voice&) = delete;
    Voice& operator=(const Voice&) = delete;

    bool active = false;
    bool isLooping = false;
    bool stolen = false;
    int note = -1;
    int velocity = 0;
    const SampleData* sample = nullptr;
    InstrumentData* owner = nullptr;
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

    // Last raw sample, held through a DFD underrun fade instead of dropping to zero
    float lastL = 0.0f;
    float lastR = 0.0f;
    bool underrun = false;

    // Per-voice stereo filter (for cutoff & resonance timbre shaping)
    Biquad filter[2];

    // DFD Streaming Components
    std::unique_ptr<VoiceRingBuffer> ringBuffer;
    size_t nextDiskFrame = 0;
    size_t ringBufferHeadFrame = 0;
};

// Threading contract:
//  - noteOn / noteOff / setSustainPedal / releaseAll / allNotesOff / render: audio thread only.
//  - loadInstrument, presets and parameter setters: any non-audio thread.
class Sampler : public StreamClient {
public:
    Sampler(size_t maxPolyphony = 128, float sampleRate = 44100.0f, bool fullyPreload = false);
    ~Sampler() override;

    void prepare(float sampleRate);

    bool loadInstrument(const std::string& jsonPath, const std::string& samplesDir);

    void noteOn(int note, int velocity, bool looping = false);
    void noteOff(int note);
    void setSustainPedal(bool pressed);
    void releaseAll(float fadeMs);
    void allNotesOff();

    void render(float* outputBuffer, uint32_t numFrames, uint32_t numChannels);

    int getActiveVoiceCount() const { return m_activeVoices.load(std::memory_order_relaxed); }
    std::string getInstrumentName() const;
    void setMasterVolume(float vol);
    float getMasterVolume() const { return m_masterVolume.load(); }

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

    // StreamClient (I/O thread)
    StreamSlot* getStreamSlots(size_t& count) override;
    void collectGarbage() override;

private:
    static constexpr size_t kStealReserve = 8; // spare voices so a stolen voice can fade instead of cutting

    std::shared_ptr<SampleData> loadSampleFile(const std::string& samplesDir, const std::string& filename) const;
    const VelocityLayer* findBestLayer(const InstrumentData& inst, int note, int velocity, int& outRootNote) const;
    void adoptPendingInstrument();
    void stopVoice(Voice& v, size_t idx);
    void requestStream(Voice& v, size_t idx);
    int pickStealVictim() const;

    float m_sampleRate = 44100.0f;
    bool m_fullyPreload = false;

    // Instrument handoff: loader -> m_pending -> (audio thread) m_current
    std::atomic<InstrumentData*> m_pending{nullptr};
    InstrumentData* m_current = nullptr; // audio thread only
    std::mutex m_ownedMutex;             // loader + I/O thread
    std::vector<std::unique_ptr<InstrumentData>> m_owned;

    std::vector<Voice> m_voices;
    std::unique_ptr<StreamSlot[]> m_slots;
    size_t m_maxPolyphony;
    std::atomic<int> m_activeVoices{0};

    bool m_sustainPedal = false;
    uint64_t m_voiceAgeCounter = 0;

    // Audio-thread readable parameters
    std::atomic<float> m_filterCutoff{20000.0f};
    std::atomic<float> m_filterResonance{0.0f};
    std::atomic<float> m_attackMs{5.0f};
    std::atomic<float> m_decayMs{250.0f};
    std::atomic<float> m_sustainLevel{1.0f};
    std::atomic<float> m_releaseMs{600.0f};
    std::atomic<float> m_masterVolume{1.0f};
    std::atomic<bool> m_eqEnabled{true};
    std::atomic<bool> m_compEnabled{true};

    // Timbre, EQ & Dynamics Engines
    TimbrePreset m_preset;               // guarded by m_presetMutex, never read by render
    mutable std::mutex m_presetMutex;
    std::string m_instrumentName = "None";
    std::string m_samplesDir;
    Equalizer m_equalizer;
    Compressor m_compressor;
};
