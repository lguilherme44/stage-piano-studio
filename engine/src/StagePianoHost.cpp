#include "StagePianoHost.hpp"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

StagePianoHost::StagePianoHost(float sampleRate, const std::string& libraryDir)
    : m_sampleRate(sampleRate),
      m_libDir(libraryDir),
      m_equalizer(sampleRate),
      m_chorus(sampleRate),
      m_delay(sampleRate),
      m_reverb(sampleRate),
      m_compressor(sampleRate) {
    m_samplesDir = m_libDir + "/samples";
    m_instDir = m_libDir + "/instruments";

    m_pianoSampler = std::make_shared<Sampler>(64, sampleRate);
    m_synthSampler = std::make_shared<Sampler>(64, sampleRate);
    m_droneSampler = std::make_shared<Sampler>(8, sampleRate);

    // Initial Defaults
    loadPiano("NS3_Bright_Grand");
    loadSynth("Warm_and_Soft");
    loadDrone("ChillPad1");

    // Default FX Settings: Subtle studio warmth
    m_reverb.setEnabled(true);
    m_reverb.setMix(0.24f);
    m_reverb.setDecay(0.70f);

    m_chorus.setEnabled(false); // Disabled by default, enabled on Rhodes/FM
    m_chorus.setMix(0.35f);

    m_delay.setEnabled(false);
    m_delay.setMix(0.22f);

    m_compressor.setEnabled(true);
    m_compressor.setThreshold(-15.0f);
    m_compressor.setRatio(3.5f);
}

StagePianoHost::~StagePianoHost() {
    allNotesOff();
}

bool StagePianoHost::loadPiano(const std::string& instrumentName) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    std::string path = m_instDir + "/" + instrumentName + ".json";
    if (!fs::exists(path)) return false;
    return m_pianoSampler->loadInstrument(path, m_samplesDir);
}

bool StagePianoHost::loadSynth(const std::string& instrumentName) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    std::string path = m_instDir + "/" + instrumentName + ".json";
    if (!fs::exists(path)) return false;
    bool ok = m_synthSampler->loadInstrument(path, m_samplesDir);
    if (ok) {
        // Default warm pad attack/release for synth slot
        m_synthSampler->setAttackMs(25.0f);
        m_synthSampler->setReleaseMs(800.0f);
        m_synthSampler->setFilterCutoff(8500.0f);
    }
    return ok;
}

bool StagePianoHost::loadDrone(const std::string& instrumentName) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    std::string path = m_instDir + "/" + instrumentName + ".json";
    if (!fs::exists(path)) return false;
    bool ok = m_droneSampler->loadInstrument(path, m_samplesDir);
    if (ok && m_droneActive.load()) {
        triggerDroneKey(m_droneRootNote);
    }
    return ok;
}

void StagePianoHost::triggerDroneKey(int rootNote) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    if (m_droneActive.load() && m_droneRootNote != rootNote) {
        m_droneSampler->noteOff(m_droneRootNote);
    }
    m_droneRootNote = rootNote;
    m_droneActive.store(true);

    m_droneSampler->setAttackMs(800.0f);     // Smooth slow swell
    m_droneSampler->setReleaseMs(3500.0f);   // Long ambient fade
    m_droneSampler->setFilterCutoff(4500.0f);// Soft warm bed
    m_droneSampler->noteOn(rootNote, 90, true);
}

void StagePianoHost::stopDrone() {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_droneActive.store(false);
    m_droneSampler->noteOff(m_droneRootNote);
}

void StagePianoHost::toggleDrone(int rootNote) {
    if (m_droneActive.load() && m_droneRootNote == rootNote) {
        stopDrone();
    } else {
        triggerDroneKey(rootNote);
    }
}

void StagePianoHost::setDroneCutoff(float hz) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_droneSampler->setFilterCutoff(hz);
}

void StagePianoHost::noteOn(int note, int velocity) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);

    if (m_pianoEnabled) {
        int pianoNote = std::clamp(note + (m_pianoOctave * 12), 0, 127);
        m_pianoSampler->noteOn(pianoNote, velocity);
    }

    if (m_synthEnabled) {
        int synthNote = std::clamp(note + (m_synthOctave * 12), 0, 127);
        m_synthSampler->noteOn(synthNote, velocity);
    }
}

void StagePianoHost::noteOff(int note) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);

    if (m_pianoEnabled) {
        int pianoNote = std::clamp(note + (m_pianoOctave * 12), 0, 127);
        m_pianoSampler->noteOff(pianoNote);
    }

    if (m_synthEnabled) {
        int synthNote = std::clamp(note + (m_synthOctave * 12), 0, 127);
        m_synthSampler->noteOff(synthNote);
    }
}

void StagePianoHost::setSustainPedal(bool pressed) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_pianoSampler->setSustainPedal(pressed);
    m_synthSampler->setSustainPedal(pressed);
}

void StagePianoHost::allNotesOff() {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_pianoSampler->allNotesOff();
    m_synthSampler->allNotesOff();
    stopDrone();
}

void StagePianoHost::render(float* outputBuffer, uint32_t numFrames, uint32_t numChannels) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);

    std::fill(outputBuffer, outputBuffer + (numFrames * numChannels), 0.0f);

    // 1. Render Slot A (Piano)
    if (m_pianoEnabled && m_pianoVolume > 0.001f) {
        std::vector<float> pianoBuf(numFrames * numChannels, 0.0f);
        m_pianoSampler->render(pianoBuf.data(), numFrames, numChannels);
        for (uint32_t i = 0; i < numFrames * numChannels; ++i) {
            outputBuffer[i] += pianoBuf[i] * m_pianoVolume;
        }
    }

    // 2. Render Slot B (Synth / Pad)
    if (m_synthEnabled && m_synthVolume > 0.001f) {
        std::vector<float> synthBuf(numFrames * numChannels, 0.0f);
        m_synthSampler->render(synthBuf.data(), numFrames, numChannels);
        for (uint32_t i = 0; i < numFrames * numChannels; ++i) {
            outputBuffer[i] += synthBuf[i] * m_synthVolume;
        }
    }

    // 3. Render Continuous Drone / Pad Bed
    if (m_droneActive.load() && m_droneVolume > 0.001f) {
        std::vector<float> droneBuf(numFrames * numChannels, 0.0f);
        m_droneSampler->render(droneBuf.data(), numFrames, numChannels);

        for (uint32_t i = 0; i < numFrames * numChannels; ++i) {
            outputBuffer[i] += droneBuf[i] * m_droneVolume;
        }
    }

    // 4. Studio 3-Band Equalizer (Grave, Médio, Agudo)
    m_equalizer.process(outputBuffer, numFrames, numChannels);

    // 5. Stereo Dimension Chorus
    m_chorus.process(outputBuffer, numFrames, numChannels);

    // 6. Ping-Pong Delay
    m_delay.process(outputBuffer, numFrames, numChannels);

    // 7. Studio Hall Reverb
    m_reverb.process(outputBuffer, numFrames, numChannels);

    // 8. Dynamic Studio Compressor
    m_compressor.process(outputBuffer, numFrames, numChannels);

    // 9. Soft-Saturation Limiter (prevents digital overs)
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
