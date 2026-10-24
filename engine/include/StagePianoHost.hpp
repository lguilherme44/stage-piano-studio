#pragma once

#include "Sampler.hpp"
#include "Equalizer.hpp"
#include "Compressor.hpp"
#include "Chorus.hpp"
#include "Delay.hpp"
#include "Reverb.hpp"
#include "TimbrePreset.hpp"

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

class StagePianoHost {
public:
    StagePianoHost(float sampleRate = 44100.0f, const std::string& libraryDir = "/Volumes/SSD-EXT/StagePianoStudio/library");
    ~StagePianoHost();

    // Sound Layer Slots
    bool loadPiano(const std::string& instrumentName);
    bool loadSynth(const std::string& instrumentName);
    bool loadDrone(const std::string& instrumentName);

    void setLibraryDir(const std::string& libraryDir);
    const std::string& getLibraryDir() const { return m_libDir; }
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

    // Audio Rendering (Full FX Chain)
    void render(float* outputBuffer, uint32_t numFrames, uint32_t numChannels);

    // FX Chain Access
    Equalizer& getEqualizer() { return m_equalizer; }
    Compressor& getCompressor() { return m_compressor; }
    Chorus& getChorus() { return m_chorus; }
    Delay& getDelay() { return m_delay; }
    Reverb& getReverb() { return m_reverb; }

    // Direct DSP Access for Synth Layer
    void setSynthFilterCutoff(float hz) { m_synthSampler->setFilterCutoff(hz); }
    void setSynthFilterResonance(float res) { m_synthSampler->setFilterResonance(res); }
    void setSynthAttack(float ms) { m_synthSampler->setAttackMs(ms); }
    void setSynthRelease(float ms) { m_synthSampler->setReleaseMs(ms); }

    int getTotalActiveVoices() const {
        return m_pianoSampler->getActiveVoiceCount() + m_synthSampler->getActiveVoiceCount() + (m_droneActive.load() ? 1 : 0);
    }

    std::string getPianoName() const { return m_pianoSampler->getInstrumentName(); }
    std::string getSynthName() const { return m_synthSampler->getInstrumentName(); }
    std::string getDroneName() const { return m_droneSampler->getInstrumentName(); }

private:
    float m_sampleRate = 44100.0f;
    std::string m_libDir;
    std::string m_samplesDir;
    std::string m_instDir;

    // Multi-Layer Samplers
    std::shared_ptr<Sampler> m_pianoSampler; // Slot A
    std::shared_ptr<Sampler> m_synthSampler; // Slot B
    std::shared_ptr<Sampler> m_droneSampler; // Ambient Continuous Drone

    // Slot Controls
    bool m_pianoEnabled = true;
    float m_pianoVolume = 1.0f;
    int m_pianoOctave = 0;

    bool m_synthEnabled = true;
    float m_synthVolume = 0.65f;
    int m_synthOctave = 0;

    // Drone state
    std::atomic<bool> m_droneActive{false};
    float m_droneVolume = 0.70f;
    int m_droneRootNote = 36; // C2

    // Studio FX Chain
    Equalizer m_equalizer;
    Chorus m_chorus;
    Delay m_delay;
    Reverb m_reverb;
    Compressor m_compressor;

    std::recursive_mutex m_hostMutex;
};
