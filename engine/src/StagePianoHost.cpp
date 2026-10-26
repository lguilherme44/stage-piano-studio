#include "StagePianoHost.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

namespace {
constexpr uint32_t kMaxChannels = 8;
constexpr float kGainSmoothSeconds = 0.020f;
constexpr float kLayerOffFadeMs = 20.0f;
}

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
    m_droneSampler = std::make_shared<Sampler>(8, sampleRate, true); // whole samples in RAM for the loop crossfade

    prepare(sampleRate, 1024);

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

StagePianoHost::~StagePianoHost() = default;

void StagePianoHost::prepare(float sampleRate, uint32_t maxBlockSize) {
    // Audio is stopped here, so events may have piled up: keep state changes (note-offs, pedal,
    // drone latch, panic) but drop note-ons, which would otherwise all fire at once as stale notes
    Event e;
    while (m_events.pop(e)) {
        if (e.type != Event::NoteOn) applyEvent(e);
    }

    if (sampleRate != m_sampleRate) {
        // Playing voices hold pitch and envelope steps computed for the old rate
        m_pianoSampler->allNotesOff();
        m_synthSampler->allNotesOff();
        m_droneSampler->allNotesOff();
        if (m_droneActive.load()) {
            pushEvent({Event::DroneOn, DroneLayer, static_cast<int16_t>(m_droneRootNote.load()), 90});
        }

        m_sampleRate = sampleRate;
        m_pianoSampler->prepare(sampleRate);
        m_synthSampler->prepare(sampleRate);
        m_droneSampler->prepare(sampleRate);
        m_pianoEq.setSampleRate(sampleRate);
        m_synthEq.setSampleRate(sampleRate);
        m_equalizer.setSampleRate(sampleRate);
        m_compressor.setSampleRate(sampleRate);
        m_chorus.prepare(sampleRate);
        m_delay.prepare(sampleRate);
        m_reverb.prepare(sampleRate);
    }

    size_t samples = static_cast<size_t>(std::max<uint32_t>(maxBlockSize, 64)) * kMaxChannels;
    m_mixBuffer.assign(samples, 0.0f);
    m_layerBuffer.assign(samples, 0.0f);
    m_gainSmoothCoeff = 1.0f - std::exp(-1.0f / (kGainSmoothSeconds * m_sampleRate));
}

void StagePianoHost::setLibraryDir(const std::string& libraryDir) {
    std::lock_guard<std::mutex> lock(m_pathMutex);
    m_libDir = libraryDir;
    m_samplesDir = m_libDir + "/samples";
    m_instDir = m_libDir + "/instruments";
}

std::string StagePianoHost::getLibraryDir() const {
    std::lock_guard<std::mutex> lock(m_pathMutex);
    return m_libDir;
}

bool StagePianoHost::isInstrumentInstalled(const std::string& instrumentName) const {
    std::string jsonPath, samplesDir;
    if (!resolveInstrument(instrumentName, jsonPath, samplesDir)) return false;

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
                    if (fs::exists(samplesDir + "/" + sampleFile)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool StagePianoHost::resolveInstrument(const std::string& instrumentName, std::string& jsonPath, std::string& samplesDir) const {
    {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        jsonPath = m_instDir + "/" + instrumentName + ".json";
        samplesDir = m_samplesDir;
    }
    return fs::exists(jsonPath);
}

void StagePianoHost::pushEvent(const Event& e) {
    if (!m_events.push(e)) {
        m_droppedEvents.fetch_add(1, std::memory_order_relaxed);
    }
}

void StagePianoHost::setPianoEnabled(bool enabled) {
    m_pianoEnabled = enabled;
    if (!enabled) pushEvent({Event::ReleaseLayer, PianoLayer, 0, 0});
}

void StagePianoHost::setSynthEnabled(bool enabled) {
    m_synthEnabled = enabled;
    if (!enabled) pushEvent({Event::ReleaseLayer, SynthLayer, 0, 0});
}

bool StagePianoHost::loadPiano(const std::string& instrumentName) {
    std::string jsonPath, samplesDir;
    if (!resolveInstrument(instrumentName, jsonPath, samplesDir)) return false;
    return m_pianoSampler->loadInstrument(jsonPath, samplesDir);
}

bool StagePianoHost::loadSynth(const std::string& instrumentName) {
    std::string jsonPath, samplesDir;
    if (!resolveInstrument(instrumentName, jsonPath, samplesDir)) return false;
    bool ok = m_synthSampler->loadInstrument(jsonPath, samplesDir);
    if (ok) {
        // Default warm pad attack/release for synth slot
        m_synthSampler->setAttackMs(25.0f);
        m_synthSampler->setReleaseMs(800.0f);
        m_synthSampler->setFilterCutoff(8500.0f);
    }
    return ok;
}

bool StagePianoHost::loadDrone(const std::string& instrumentName) {
    std::string jsonPath, samplesDir;
    if (!resolveInstrument(instrumentName, jsonPath, samplesDir)) return false;
    bool ok = m_droneSampler->loadInstrument(jsonPath, samplesDir);
    if (ok && m_droneActive.load()) {
        triggerDroneKey(m_droneRootNote);
    }
    return ok;
}

void StagePianoHost::triggerDroneKey(int rootNote) {
    int previous = m_droneRootNote.exchange(rootNote);
    if (m_droneActive.load() && previous != rootNote) {
        pushEvent({Event::DroneOff, DroneLayer, static_cast<int16_t>(previous), 0});
    }
    m_droneActive.store(true);

    m_droneSampler->setAttackMs(800.0f);     // Smooth slow swell
    m_droneSampler->setReleaseMs(3500.0f);   // Long ambient fade
    m_droneSampler->setFilterCutoff(4500.0f);// Soft warm bed
    pushEvent({Event::DroneOn, DroneLayer, static_cast<int16_t>(rootNote), 90});
}

void StagePianoHost::stopDrone() {
    m_droneActive.store(false);
    // Note off, not a cut: the 3.5s release keeps rendering until the voice fades out
    pushEvent({Event::DroneOff, DroneLayer, static_cast<int16_t>(m_droneRootNote.load()), 0});
}

void StagePianoHost::toggleDrone(int rootNote) {
    if (m_droneActive.load() && m_droneRootNote == rootNote) {
        stopDrone();
    } else {
        triggerDroneKey(rootNote);
    }
}

void StagePianoHost::setDroneCutoff(float hz) {
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
    m_stringResonance = enabled;
    m_pianoSampler->setReleaseMs(enabled ? 1200.0f : 450.0f);
}

void StagePianoHost::setSoftPiano(bool enabled) {
    m_softPiano = enabled;
    m_pianoSampler->setFilterCutoff(enabled ? 5500.0f : 20000.0f);
}

void StagePianoHost::noteOn(int note, int velocity) {
    if (m_learningLowestC.load()) {
        processMidiNoteForCalibration(note);
        return;
    }
    int mappedVel = applyVelocityCurveMapping(velocity, m_velocityCurve);
    int transposedNote = std::clamp(note + m_globalTranspose + (m_globalOctave * 12), 0, 127);

    if (m_pianoEnabled) {
        int pianoNote = std::clamp(transposedNote + (m_pianoOctave * 12), 0, 127);
        pushEvent({Event::NoteOn, PianoLayer, static_cast<int16_t>(pianoNote), static_cast<int16_t>(mappedVel)});
    }

    if (m_synthEnabled) {
        int synthNote = std::clamp(transposedNote + (m_synthOctave * 12), 0, 127);
        pushEvent({Event::NoteOn, SynthLayer, static_cast<int16_t>(synthNote), static_cast<int16_t>(mappedVel)});
    }
}

void StagePianoHost::setParamChangeListener(std::function<void(MidiCCParam, float)> callback) {
    std::lock_guard<std::mutex> lock(m_ccMutex);
    m_paramCallback = std::move(callback);
}

void StagePianoHost::setMidiBinding(int controller, MidiCCParam param) {
    std::lock_guard<std::mutex> lock(m_ccMutex);
    if (param == MidiCCParam::None) {
        m_ccBindings.erase(controller);
    } else {
        m_ccBindings[controller] = param;
    }
}

MidiCCParam StagePianoHost::getMidiBinding(int controller) const {
    std::lock_guard<std::mutex> lock(m_ccMutex);
    auto it = m_ccBindings.find(controller);
    return (it != m_ccBindings.end()) ? it->second : MidiCCParam::None;
}

void StagePianoHost::handleMidiCC(int controller, int value) {
    std::lock_guard<std::mutex> lock(m_ccMutex);

    MidiCCParam learning = m_learningParam.exchange(MidiCCParam::None);
    if (learning != MidiCCParam::None) {
        m_ccBindings[controller] = learning;
        if (m_paramCallback) {
            float norm = static_cast<float>(value) / 127.0f;
            m_paramCallback(learning, norm);
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
    int transposedNote = std::clamp(note + m_globalTranspose + (m_globalOctave * 12), 0, 127);

    // Sent to both layers even if one was just disabled, so no note is left hanging
    int pianoNote = std::clamp(transposedNote + (m_pianoOctave * 12), 0, 127);
    pushEvent({Event::NoteOff, PianoLayer, static_cast<int16_t>(pianoNote), 0});

    int synthNote = std::clamp(transposedNote + (m_synthOctave * 12), 0, 127);
    pushEvent({Event::NoteOff, SynthLayer, static_cast<int16_t>(synthNote), 0});
}

void StagePianoHost::panic() {
    m_droneActive.store(false);
    pushEvent({Event::Panic, 0, 0, 0});
    std::cout << "[StagePianoHost] EMERGENCY PANIC TRIGGERED: All voices, DFD streams & DSP buffers flushed.\n";
}

void StagePianoHost::setSustainPedal(bool pressed) {
    pushEvent({Event::Sustain, 0, 0, static_cast<int16_t>(pressed ? 1 : 0)});
}

void StagePianoHost::allNotesOff() {
    pushEvent({Event::AllNotesOff, 0, 0, 0});
    stopDrone();
}

void StagePianoHost::applyEvent(const Event& e) {
    Sampler* layers[LayerCount] = {m_pianoSampler.get(), m_synthSampler.get(), m_droneSampler.get()};
    switch (e.type) {
        case Event::NoteOn:
            layers[e.layer]->noteOn(e.note, e.value);
            break;
        case Event::NoteOff:
            layers[e.layer]->noteOff(e.note);
            break;
        case Event::Sustain:
            m_pianoSampler->setSustainPedal(e.value != 0);
            m_synthSampler->setSustainPedal(e.value != 0);
            break;
        case Event::ReleaseLayer:
            layers[e.layer]->releaseAll(kLayerOffFadeMs);
            break;
        case Event::AllNotesOff:
            m_pianoSampler->releaseAll(kLayerOffFadeMs);
            m_synthSampler->releaseAll(kLayerOffFadeMs);
            break;
        case Event::Panic:
            m_pianoSampler->allNotesOff();
            m_synthSampler->allNotesOff();
            m_droneSampler->allNotesOff();
            m_reverb.reset();
            m_delay.reset();
            m_chorus.reset();
            m_pianoEq.reset();
            m_synthEq.reset();
            m_equalizer.reset();
            break;
        case Event::DroneOn:
            m_droneSampler->noteOn(e.note, e.value, true);
            break;
        case Event::DroneOff:
            m_droneSampler->noteOff(e.note);
            break;
    }
}

void StagePianoHost::render(float* outputBuffer, uint32_t numFrames, uint32_t numChannels) {
    if (numChannels == 0 || numChannels > kMaxChannels) {
        std::fill(outputBuffer, outputBuffer + (numFrames * numChannels), 0.0f);
        return;
    }

    Event e;
    while (m_events.pop(e)) {
        applyEvent(e);
    }

    // Process in chunks that fit the preallocated buffers: no allocation on the audio thread
    const uint32_t maxChunk = static_cast<uint32_t>(m_mixBuffer.size() / numChannels);
    uint32_t done = 0;
    while (done < numFrames) {
        uint32_t chunk = std::min(maxChunk, numFrames - done);
        renderChunk(outputBuffer + static_cast<size_t>(done) * numChannels, chunk, numChannels);
        done += chunk;
    }
}

void StagePianoHost::renderChunk(float* out, uint32_t numFrames, uint32_t numChannels) {
    const uint32_t totalSamples = numFrames * numChannels;
    std::fill(out, out + totalSamples, 0.0f);

    Sampler* samplers[LayerCount] = {m_pianoSampler.get(), m_synthSampler.get(), m_droneSampler.get()};
    Equalizer* layerEq[LayerCount] = {&m_pianoEq, &m_synthEq, nullptr};
    const float targets[LayerCount] = {m_pianoVolume.load(), m_synthVolume.load(), m_droneVolume.load()};
    float* layerBuf = m_layerBuffer.data();

    // 1-3. Slot A (Piano), Slot B (Synth) with dedicated EQ, and the continuous Drone bed.
    // Layers keep rendering while voices fade out, even after being disabled or stopped.
    for (int l = 0; l < LayerCount; ++l) {
        std::fill(layerBuf, layerBuf + totalSamples, 0.0f);
        samplers[l]->render(layerBuf, numFrames, numChannels);
        if (layerEq[l]) layerEq[l]->process(layerBuf, numFrames, numChannels);

        // Smoothed gain: CC / slider steps ramp over ~20ms instead of zippering
        float g = m_layerGain[l];
        const float target = targets[l];
        for (uint32_t f = 0; f < numFrames; ++f) {
            g += (target - g) * m_gainSmoothCoeff;
            for (uint32_t c = 0; c < numChannels; ++c) {
                out[f * numChannels + c] += layerBuf[f * numChannels + c] * g;
            }
        }
        m_layerGain[l] = g;
    }

    // 4. Studio 3-Band Equalizer (Grave, Médio, Agudo)
    m_equalizer.process(out, numFrames, numChannels);

    // 5. Stereo Dimension Chorus
    m_chorus.process(out, numFrames, numChannels);

    // 6. Ping-Pong Delay
    m_delay.process(out, numFrames, numChannels);

    // 7. Studio Hall Reverb
    m_reverb.process(out, numFrames, numChannels);

    // 8. Dynamic Studio Compressor
    m_compressor.process(out, numFrames, numChannels);

    // 9. Studio Soft-Knee Analog Limiter / Saturator
    // Transparent up to 0.85, smooth asymptotic tanh saturation above, zero discontinuities
    const float threshold = 0.85f;
    const float headroom = 1.0f - threshold;
    for (uint32_t i = 0; i < totalSamples; ++i) {
        float s = out[i];
        if (s > threshold) {
            out[i] = threshold + headroom * std::tanh((s - threshold) / headroom);
        } else if (s < -threshold) {
            out[i] = -threshold + headroom * std::tanh((s + threshold) / headroom);
        }
    }
}
