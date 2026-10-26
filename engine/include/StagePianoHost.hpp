#pragma once

#include "Sampler.hpp"
#include "Equalizer.hpp"
#include "Compressor.hpp"
#include "Chorus.hpp"
#include "Delay.hpp"
#include "Reverb.hpp"
#include "TimbrePreset.hpp"
#include "EventQueue.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>

enum class VelocityCurve {
    Linear = 0,
    Soft1,
    Soft2,
    Hard1,
    Hard2,
    Compressed
};

enum class MidiCCParam {
    None = 0,
    PianoVolume,
    SynthVolume,
    DroneVolume,
    SynthCutoff,
    PianoEqLow,
    PianoEqMid,
    PianoEqHigh,
    SynthEqLow,
    SynthEqMid,
    SynthEqHigh,
    EqLow,
    EqMid,
    EqHigh,
    ChorusMix,
    DelayMix,
    ReverbMix,
    MasterVolume
};

// Threading model: render() runs on the audio thread and never takes a lock or allocates.
// Note/pedal/drone/panic calls from MIDI or UI threads are queued and applied at the start of
// the next render(); parameters are atomics; instrument loads happen on the calling thread.
class StagePianoHost {
public:
    StagePianoHost(float sampleRate = 44100.0f, const std::string& libraryDir = "/Volumes/SSD-EXT/StagePianoStudio/library");
    ~StagePianoHost();

    // Must be called before audio starts and whenever the device sample rate / block size changes.
    // Not concurrent with render().
    void prepare(float sampleRate, uint32_t maxBlockSize);
    float getSampleRate() const { return m_sampleRate; }

    // Sound Layer Slots
    bool loadPiano(const std::string& instrumentName);
    bool loadSynth(const std::string& instrumentName);
    bool loadDrone(const std::string& instrumentName);

    void setLibraryDir(const std::string& libraryDir);
    std::string getLibraryDir() const;
    bool isInstrumentInstalled(const std::string& instrumentName) const;

    void setPianoEnabled(bool enabled);
    bool isPianoEnabled() const { return m_pianoEnabled; }
    void setPianoVolume(float vol) { m_pianoVolume = vol; }
    float getPianoVolume() const { return m_pianoVolume; }
    void setPianoOctave(int oct) { m_pianoOctave = oct; }
    int getPianoOctave() const { return m_pianoOctave; }

    void setSynthEnabled(bool enabled);
    bool isSynthEnabled() const { return m_synthEnabled; }
    void setSynthVolume(float vol) { m_synthVolume = vol; }
    float getSynthVolume() const { return m_synthVolume; }
    void setSynthOctave(int oct) { m_synthOctave = oct; }
    int getSynthOctave() const { return m_synthOctave; }

    // Drone / Ambient Pad Engine
    void triggerDroneKey(int rootNote); // Latch drone on note (e.g. 36 = C2, 48 = C3)
    void stopDrone();
    void toggleDrone(int rootNote = 36);
    bool isDroneActive() const { return m_droneActive.load(); }
    void setDroneVolume(float vol) { m_droneVolume = vol; }
    float getDroneVolume() const { return m_droneVolume; }
    void setDroneCutoff(float hz);
    int getDroneRootNote() const { return m_droneRootNote; }

    // Real-time MIDI input
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void setSustainPedal(bool pressed);
    void allNotesOff();

    // Velocity Curve Profile
    void setVelocityCurve(VelocityCurve curve) { m_velocityCurve = curve; }
    VelocityCurve getVelocityCurve() const { return m_velocityCurve; }

    // MIDI CC Router & MIDI Learn
    void handleMidiCC(int controller, int value);
    void startMidiLearn(MidiCCParam param) { m_learningParam = param; }
    void cancelMidiLearn() { m_learningParam = MidiCCParam::None; }
    bool isMidiLearning() const { return m_learningParam != MidiCCParam::None; }
    MidiCCParam getMidiLearnTarget() const { return m_learningParam; }
    void setMidiBinding(int controller, MidiCCParam param);
    MidiCCParam getMidiBinding(int controller) const;
    void setParamChangeListener(std::function<void(MidiCCParam, float)> callback);

    // Audio Rendering (Full FX Chain)
    void render(float* outputBuffer, uint32_t numFrames, uint32_t numChannels);

    // FX Chain Access
    Equalizer& getPianoEqualizer() { return m_pianoEq; }
    Equalizer& getSynthEqualizer() { return m_synthEq; }
    Equalizer& getEqualizer() { return m_equalizer; }
    Compressor& getCompressor() { return m_compressor; }
    Chorus& getChorus() { return m_chorus; }
    Delay& getDelay() { return m_delay; }
    Reverb& getReverb() { return m_reverb; }

    // Emergency Panic & Safety Guard (Flushes all voices, DFD streams & DSP buffers)
    void panic();

    // Global Transpose (+/- 24 semitones) and Octave (+/- 3 octaves)
    void setGlobalTranspose(int semitones) { m_globalTranspose = std::clamp(semitones, -24, 24); }
    int getGlobalTranspose() const { return m_globalTranspose; }
    void setGlobalOctave(int oct) { m_globalOctave = std::clamp(oct, -3, 3); }
    int getGlobalOctave() const { return m_globalOctave; }

    // Keyboard Low-C Calibration (for 61-key keyboards like Roland XPS-10)
    void startLearningLowestC() { m_learningLowestC = true; }
    bool isLearningLowestC() const { return m_learningLowestC.load(); }
    void cancelLearningLowestC() { m_learningLowestC = false; }
    int getLowestCMappedNote() const { return m_lowestCMappedNote; }
    bool processMidiNoteForCalibration(int midiNote);

    // Acoustic Piano Timbre Features (Nord-style String Resonance & Soft Ballad Action)
    void setStringResonance(bool enabled);
    bool isStringResonanceEnabled() const { return m_stringResonance; }
    void setSoftPiano(bool enabled);
    bool isSoftPianoEnabled() const { return m_softPiano; }

    // DFD Diagnostics & Monitoring (Kontakt-aligned)
    int getDfdLoadPercent() const { return DiskStreamer::getInstance().getDfdLoadPercent(); }
    int getDfdActiveRequests() const { return DiskStreamer::getInstance().getActiveRequests(); }
    uint64_t getDfdUnderrunCount() const { return DiskStreamer::getInstance().getUnderrunCount(); }
    uint64_t getDroppedEventCount() const { return m_droppedEvents.load(std::memory_order_relaxed); }

    // Direct DSP Access for Synth Layer
    void setSynthFilterCutoff(float hz) { m_synthSampler->setFilterCutoff(hz); }
    void setSynthFilterResonance(float res) { m_synthSampler->setFilterResonance(res); }
    void setSynthAttack(float ms) { m_synthSampler->setAttackMs(ms); }
    void setSynthRelease(float ms) { m_synthSampler->setReleaseMs(ms); }

    int getTotalActiveVoices() const {
        return m_pianoSampler->getActiveVoiceCount() + m_synthSampler->getActiveVoiceCount() + m_droneSampler->getActiveVoiceCount();
    }

    std::string getPianoName() const { return m_pianoSampler->getInstrumentName(); }
    std::string getSynthName() const { return m_synthSampler->getInstrumentName(); }
    std::string getDroneName() const { return m_droneSampler->getInstrumentName(); }

private:
    enum Layer : uint8_t { PianoLayer = 0, SynthLayer = 1, DroneLayer = 2, LayerCount = 3 };

    struct Event {
        enum Type : uint8_t { NoteOn, NoteOff, Sustain, ReleaseLayer, AllNotesOff, Panic, DroneOn, DroneOff };
        Type type = NoteOn;
        uint8_t layer = 0;
        int16_t note = 0;
        int16_t value = 0;
    };

    void pushEvent(const Event& e);
    void applyEvent(const Event& e);
    void renderChunk(float* out, uint32_t numFrames, uint32_t numChannels);
    bool resolveInstrument(const std::string& instrumentName, std::string& jsonPath, std::string& samplesDir) const;

    float m_sampleRate = 44100.0f;
    std::string m_libDir;
    std::string m_samplesDir;
    std::string m_instDir;
    mutable std::mutex m_pathMutex; // library paths, never touched by render()

    // Multi-Layer Samplers
    std::shared_ptr<Sampler> m_pianoSampler; // Slot A
    std::shared_ptr<Sampler> m_synthSampler; // Slot B
    std::shared_ptr<Sampler> m_droneSampler; // Ambient Continuous Drone

    // Slot Controls
    std::atomic<bool> m_pianoEnabled{true};
    std::atomic<float> m_pianoVolume{1.0f};
    std::atomic<int> m_pianoOctave{0};

    std::atomic<bool> m_synthEnabled{true};
    std::atomic<float> m_synthVolume{0.65f};
    std::atomic<int> m_synthOctave{0};

    // Drone state
    std::atomic<bool> m_droneActive{false};
    std::atomic<float> m_droneVolume{0.70f};
    std::atomic<int> m_droneRootNote{36}; // C2

    // Studio FX Chain
    Equalizer m_pianoEq;
    Equalizer m_synthEq;
    Equalizer m_equalizer;
    Chorus m_chorus;
    Delay m_delay;
    Reverb m_reverb;
    Compressor m_compressor;

    // Velocity curve setting
    std::atomic<VelocityCurve> m_velocityCurve{VelocityCurve::Linear};
    std::atomic<int> m_globalTranspose{0};
    std::atomic<int> m_globalOctave{0};
    std::atomic<bool> m_learningLowestC{false};
    std::atomic<int> m_lowestCMappedNote{36}; // C2 by default on 61-key keyboards
    std::atomic<bool> m_stringResonance{true};
    std::atomic<bool> m_softPiano{false};

    // MIDI CC Router & Learn
    std::atomic<MidiCCParam> m_learningParam{MidiCCParam::None};
    std::map<int, MidiCCParam> m_ccBindings;
    std::function<void(MidiCCParam, float)> m_paramCallback = nullptr;
    mutable std::mutex m_ccMutex; // MIDI + UI threads only

    // Audio-thread state
    EventQueue<Event> m_events{8192}; // ~2000 keystrokes of headroom while audio is stalled
    std::atomic<uint64_t> m_droppedEvents{0};
    std::vector<float> m_mixBuffer;
    std::vector<float> m_layerBuffer;
    float m_layerGain[LayerCount] = {0.0f, 0.0f, 0.0f};
    float m_gainSmoothCoeff = 0.0f;
};
