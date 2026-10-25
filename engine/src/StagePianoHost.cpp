#include "StagePianoHost.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

StagePianoHost::StagePianoHost(float sampleRate, const std::string& libraryDir)
    : m_sampleRate(sampleRate),
      m_libDir(libraryDir),
      m_pianoEq(sampleRate),
      m_synthEq(sampleRate),
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

    // Default MIDI CC Mapping
    m_ccBindings[7]  = MidiCCParam::PianoVolume;  // Standard Volume -> Slot A
    m_ccBindings[11] = MidiCCParam::SynthVolume;  // Expression -> Slot B
    m_ccBindings[74] = MidiCCParam::SynthCutoff;  // Brightness -> Cutoff
    m_ccBindings[1]  = MidiCCParam::DroneVolume;  // Mod Wheel -> Drone Volume
    m_ccBindings[91] = MidiCCParam::ReverbMix;    // Reverb Send
    m_ccBindings[93] = MidiCCParam::ChorusMix;    // Chorus Send
}

StagePianoHost::~StagePianoHost() {
    allNotesOff();
}

void StagePianoHost::setLibraryDir(const std::string& libraryDir) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_libDir = libraryDir;
    m_samplesDir = m_libDir + "/samples";
    m_instDir = m_libDir + "/instruments";
}

bool StagePianoHost::isInstrumentInstalled(const std::string& instrumentName) const {
    std::string jsonPath = m_instDir + "/" + instrumentName + ".json";
    if (!fs::exists(jsonPath)) return false;

    std::ifstream in(jsonPath);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.find("\"file\":") != std::string::npos || line.find("\"filename\":") != std::string::npos) {
            size_t q1 = line.find('"', line.find(':'));
            if (q1 != std::string::npos) {
                size_t start = q1 + 1;
                size_t end = line.rfind('"');
                if (end > start) {
                    std::string sampleFile = line.substr(start, end - start);
                    if (fs::exists(m_samplesDir + "/" + sampleFile)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void StagePianoHost::setPianoEnabled(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_pianoEnabled = enabled;
    if (!enabled && m_pianoSampler) {
        m_pianoSampler->allNotesOff();
    }
}

void StagePianoHost::setSynthEnabled(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_synthEnabled = enabled;
    if (!enabled && m_synthSampler) {
        m_synthSampler->allNotesOff();
    }
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

static int applyVelocityCurveMapping(int inputVel, VelocityCurve curve) {
    if (inputVel <= 1) return 1;
    if (inputVel >= 127) return 127;
    float norm = static_cast<float>(inputVel) / 127.0f;
    float outNorm = norm;
    switch (curve) {
        case VelocityCurve::Linear:
            outNorm = norm;
            break;
        case VelocityCurve::Soft1:
            outNorm = std::pow(norm, 0.75f);
            break;
        case VelocityCurve::Soft2:
            outNorm = std::pow(norm, 0.55f);
            break;
        case VelocityCurve::Hard1:
            outNorm = std::pow(norm, 1.35f);
            break;
        case VelocityCurve::Hard2:
            outNorm = std::pow(norm, 1.70f);
            break;
        case VelocityCurve::Compressed:
            outNorm = 0.30f + norm * 0.70f;
            break;
    }
    return std::clamp(static_cast<int>(std::round(outNorm * 127.0f)), 1, 127);
}

bool StagePianoHost::processMidiNoteForCalibration(int midiNote) {
    if (m_learningLowestC.load()) {
        m_lowestCMappedNote = midiNote;
        // Standard concert grand bottom C is C1 (note 24)
        // If user presses XPS-10 lowest C (note 36), diff is 24 - 36 = -12 semitones (-1 octave)
        int semitoneDiff = 24 - midiNote;
        m_globalOctave = std::clamp(semitoneDiff / 12, -3, 3);
        m_learningLowestC = false;
        return true;
    }
    return false;
}

void StagePianoHost::setStringResonance(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_stringResonance = enabled;
    if (m_pianoSampler) {
        m_pianoSampler->setReleaseMs(enabled ? 1200.0f : 450.0f);
    }
}

void StagePianoHost::setSoftPiano(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_softPiano = enabled;
    if (m_pianoSampler) {
        m_pianoSampler->setFilterCutoff(enabled ? 5500.0f : 20000.0f);
    }
}

void StagePianoHost::noteOn(int note, int velocity) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    if (m_learningLowestC.load()) {
        processMidiNoteForCalibration(note);
        return;
    }
    int mappedVel = applyVelocityCurveMapping(velocity, m_velocityCurve);
    int transposedNote = std::clamp(note + m_globalTranspose + (m_globalOctave * 12), 0, 127);

    if (m_pianoEnabled) {
        int pianoNote = std::clamp(transposedNote + (m_pianoOctave * 12), 0, 127);
        m_pianoSampler->noteOn(pianoNote, mappedVel);
    }

    if (m_synthEnabled) {
        int synthNote = std::clamp(transposedNote + (m_synthOctave * 12), 0, 127);
        m_synthSampler->noteOn(synthNote, mappedVel);
    }
}

void StagePianoHost::setMidiBinding(int controller, MidiCCParam param) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    if (param == MidiCCParam::None) {
        m_ccBindings.erase(controller);
    } else {
        m_ccBindings[controller] = param;
    }
}

MidiCCParam StagePianoHost::getMidiBinding(int controller) const {
    auto it = m_ccBindings.find(controller);
    return (it != m_ccBindings.end()) ? it->second : MidiCCParam::None;
}

void StagePianoHost::handleMidiCC(int controller, int value) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);

    if (m_learningParam != MidiCCParam::None) {
        m_ccBindings[controller] = m_learningParam;
        MidiCCParam learned = m_learningParam;
        m_learningParam = MidiCCParam::None;
        if (m_paramCallback) {
            float norm = static_cast<float>(value) / 127.0f;
            m_paramCallback(learned, norm);
        }
        return;
    }

    auto it = m_ccBindings.find(controller);
    if (it == m_ccBindings.end()) return;

    float norm = static_cast<float>(value) / 127.0f;
    switch (it->second) {
        case MidiCCParam::PianoVolume:
            setPianoVolume(norm * 1.5f);
            if (m_paramCallback) m_paramCallback(MidiCCParam::PianoVolume, m_pianoVolume);
            break;
        case MidiCCParam::SynthVolume:
            setSynthVolume(norm * 1.5f);
            if (m_paramCallback) m_paramCallback(MidiCCParam::SynthVolume, m_synthVolume);
            break;
        case MidiCCParam::DroneVolume:
            setDroneVolume(norm * 1.5f);
            if (m_paramCallback) m_paramCallback(MidiCCParam::DroneVolume, m_droneVolume);
            break;
        case MidiCCParam::SynthCutoff: {
            float hz = 20.0f + (norm * norm * 19980.0f);
            setSynthFilterCutoff(hz);
            if (m_paramCallback) m_paramCallback(MidiCCParam::SynthCutoff, hz);
            break;
        }
        case MidiCCParam::PianoEqLow: {
            float db = (norm - 0.5f) * 24.0f;
            m_pianoEq.setLowGain(db);
            if (m_paramCallback) m_paramCallback(MidiCCParam::PianoEqLow, db);
            break;
        }
        case MidiCCParam::PianoEqMid: {
            float db = (norm - 0.5f) * 24.0f;
            m_pianoEq.setMidGain(db);
            if (m_paramCallback) m_paramCallback(MidiCCParam::PianoEqMid, db);
            break;
        }
        case MidiCCParam::PianoEqHigh: {
            float db = (norm - 0.5f) * 24.0f;
            m_pianoEq.setHighGain(db);
            if (m_paramCallback) m_paramCallback(MidiCCParam::PianoEqHigh, db);
            break;
        }
        case MidiCCParam::SynthEqLow: {
            float db = (norm - 0.5f) * 24.0f;
            m_synthEq.setLowGain(db);
            if (m_paramCallback) m_paramCallback(MidiCCParam::SynthEqLow, db);
            break;
        }
        case MidiCCParam::SynthEqMid: {
            float db = (norm - 0.5f) * 24.0f;
            m_synthEq.setMidGain(db);
            if (m_paramCallback) m_paramCallback(MidiCCParam::SynthEqMid, db);
            break;
        }
        case MidiCCParam::SynthEqHigh: {
            float db = (norm - 0.5f) * 24.0f;
            m_synthEq.setHighGain(db);
            if (m_paramCallback) m_paramCallback(MidiCCParam::SynthEqHigh, db);
            break;
        }
        case MidiCCParam::EqLow: {
            float db = (norm - 0.5f) * 24.0f;
            m_equalizer.setLowGain(db);
            if (m_paramCallback) m_paramCallback(MidiCCParam::EqLow, db);
            break;
        }
        case MidiCCParam::EqMid: {
            float db = (norm - 0.5f) * 24.0f;
            m_equalizer.setMidGain(db);
            if (m_paramCallback) m_paramCallback(MidiCCParam::EqMid, db);
            break;
        }
        case MidiCCParam::EqHigh: {
            float db = (norm - 0.5f) * 24.0f;
            m_equalizer.setHighGain(db);
            if (m_paramCallback) m_paramCallback(MidiCCParam::EqHigh, db);
            break;
        }
        case MidiCCParam::ChorusMix:
            m_chorus.setMix(norm);
            if (m_paramCallback) m_paramCallback(MidiCCParam::ChorusMix, norm);
            break;
        case MidiCCParam::DelayMix:
            m_delay.setMix(norm * 0.8f);
            if (m_paramCallback) m_paramCallback(MidiCCParam::DelayMix, norm * 0.8f);
            break;
        case MidiCCParam::ReverbMix:
            m_reverb.setMix(norm * 0.8f);
            if (m_paramCallback) m_paramCallback(MidiCCParam::ReverbMix, norm * 0.8f);
            break;
        case MidiCCParam::MasterVolume:
            if (m_paramCallback) m_paramCallback(MidiCCParam::MasterVolume, norm);
            break;
        default:
            break;
    }
}

void StagePianoHost::noteOff(int note) {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    int transposedNote = std::clamp(note + m_globalTranspose + (m_globalOctave * 12), 0, 127);

    if (m_pianoEnabled) {
        int pianoNote = std::clamp(transposedNote + (m_pianoOctave * 12), 0, 127);
        m_pianoSampler->noteOff(pianoNote);
    }

    if (m_synthEnabled) {
        int synthNote = std::clamp(transposedNote + (m_synthOctave * 12), 0, 127);
        m_synthSampler->noteOff(synthNote);
    }
}

void StagePianoHost::panic() {
    std::lock_guard<std::recursive_mutex> lock(m_hostMutex);
    m_pianoSampler->allNotesOff();
    m_synthSampler->allNotesOff();
    m_droneSampler->allNotesOff();
    m_droneActive.store(false);
    m_reverb.reset();
    m_delay.reset();
    m_chorus.reset();
    m_pianoEq.reset();
    m_synthEq.reset();
    m_equalizer.reset();
    std::cout << "[StagePianoHost] EMERGENCY PANIC TRIGGERED: All voices, DFD streams & DSP buffers flushed.\n";
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

    // 1. Render Slot A (Piano) with Dedicated Slot EQ
    if (m_pianoEnabled && m_pianoVolume > 0.001f) {
        std::vector<float> pianoBuf(numFrames * numChannels, 0.0f);
        m_pianoSampler->render(pianoBuf.data(), numFrames, numChannels);
        m_pianoEq.process(pianoBuf.data(), numFrames, numChannels);
        for (uint32_t i = 0; i < numFrames * numChannels; ++i) {
            outputBuffer[i] += pianoBuf[i] * m_pianoVolume;
        }
    }

    // 2. Render Slot B (Synth / Pad) with Dedicated Slot EQ
    if (m_synthEnabled && m_synthVolume > 0.001f) {
        std::vector<float> synthBuf(numFrames * numChannels, 0.0f);
        m_synthSampler->render(synthBuf.data(), numFrames, numChannels);
        m_synthEq.process(synthBuf.data(), numFrames, numChannels);
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

    // 9. Studio Soft-Knee Analog Limiter / Saturator
    // Transparent up to 0.85, smooth asymptotic tanh saturation above, zero discontinuities
    const uint32_t totalSamples = numFrames * numChannels;
    const float threshold = 0.85f;
    const float headroom = 1.0f - threshold;
    for (uint32_t i = 0; i < totalSamples; ++i) {
        float s = outputBuffer[i];
        if (s > threshold) {
            outputBuffer[i] = threshold + headroom * std::tanh((s - threshold) / headroom);
        } else if (s < -threshold) {
            outputBuffer[i] = -threshold + headroom * std::tanh((s + threshold) / headroom);
        }
    }
}
