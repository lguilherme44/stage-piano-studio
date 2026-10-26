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

Sampler::Sampler(size_t maxPolyphony, float sampleRate, bool fullyPreload)
    : m_sampleRate(sampleRate),
      m_fullyPreload(fullyPreload),
      m_maxPolyphony(maxPolyphony),
      m_equalizer(sampleRate),
      m_compressor(sampleRate) {
    m_voices.resize(m_maxPolyphony + kStealReserve);
    m_slots = std::make_unique<StreamSlot[]>(m_voices.size());
    applyPreset(m_preset);
    DiskStreamer::getInstance().registerClient(this);
}

Sampler::~Sampler() {
    // Blocks until the I/O thread is outside its scan, so slots and samples stay valid for it
    DiskStreamer::getInstance().unregisterClient(this);
}

void Sampler::prepare(float sampleRate) {
    m_sampleRate = sampleRate;
    m_equalizer.setSampleRate(sampleRate);
    m_compressor.setSampleRate(sampleRate);
}

void Sampler::applyPreset(const TimbrePreset& preset) {
    std::lock_guard<std::mutex> lock(m_presetMutex);
    m_preset = preset;

    m_filterCutoff = std::clamp(preset.filterCutoff, 20.0f, 20000.0f);
    m_filterResonance = std::clamp(preset.filterResonance, 0.0f, 0.95f);
    m_attackMs = std::clamp(preset.attackMs, 0.0f, 5000.0f);
    m_decayMs = preset.decayMs;
    m_sustainLevel = preset.sustainLevel;
    m_releaseMs = std::clamp(preset.releaseMs, 20.0f, 5000.0f);
    m_masterVolume = std::clamp(preset.masterVolume, 0.0f, 2.0f);
    m_eqEnabled = preset.eqEnabled;
    m_compEnabled = preset.compEnabled;

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
    std::lock_guard<std::mutex> lock(m_presetMutex);
    TimbrePreset p = m_preset;
    p.filterCutoff = m_filterCutoff;
    p.filterResonance = m_filterResonance;
    p.attackMs = m_attackMs;
    p.releaseMs = m_releaseMs;
    p.masterVolume = m_masterVolume;
    return p;
}

bool Sampler::savePreset(const std::string& filepath) {
    TimbrePreset p = getCurrentPreset();
    {
        std::lock_guard<std::mutex> lock(m_presetMutex);
        p.baseInstrument = m_instrumentName;
    }
    return p.saveToFile(filepath);
}

bool Sampler::loadPreset(const std::string& filepath) {
    TimbrePreset p;
    if (!p.loadFromFile(filepath)) {
        return false;
    }
    applyPreset(p);
    return true;
}

std::string Sampler::getInstrumentName() const {
    std::lock_guard<std::mutex> lock(m_presetMutex);
    return m_instrumentName;
}

void Sampler::setFilterCutoff(float hz) {
    m_filterCutoff = std::clamp(hz, 20.0f, 20000.0f);
}

void Sampler::setFilterResonance(float res) {
    m_filterResonance = std::clamp(res, 0.0f, 0.95f);
}

void Sampler::setAttackMs(float ms) {
    m_attackMs = std::clamp(ms, 0.0f, 5000.0f);
}

void Sampler::setReleaseMs(float ms) {
    m_releaseMs = std::clamp(ms, 20.0f, 5000.0f);
}

void Sampler::setLowGain(float dB) {
    std::lock_guard<std::mutex> lock(m_presetMutex);
    m_preset.eqLowGainDb = dB;
    m_equalizer.setLowGain(dB);
}

void Sampler::setMidGain(float dB) {
    std::lock_guard<std::mutex> lock(m_presetMutex);
    m_preset.eqMidGainDb = dB;
    m_equalizer.setMidGain(dB);
}

void Sampler::setHighGain(float dB) {
    std::lock_guard<std::mutex> lock(m_presetMutex);
    m_preset.eqHighGainDb = dB;
    m_equalizer.setHighGain(dB);
}

void Sampler::setCompressorEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_presetMutex);
    m_preset.compEnabled = enabled;
    m_compEnabled = enabled;
    m_compressor.setEnabled(enabled);
}

void Sampler::setCompressorThreshold(float dB) {
    std::lock_guard<std::mutex> lock(m_presetMutex);
    m_preset.compThresholdDb = dB;
    m_compressor.setThreshold(dB);
}

void Sampler::setCompressorRatio(float ratio) {
    std::lock_guard<std::mutex> lock(m_presetMutex);
    m_preset.compRatio = ratio;
    m_compressor.setRatio(ratio);
}

void Sampler::setCompressorMakeup(float dB) {
    std::lock_guard<std::mutex> lock(m_presetMutex);
    m_preset.compMakeupDb = dB;
    m_compressor.setMakeupGain(dB);
}

void Sampler::setMasterVolume(float vol) {
    m_masterVolume = std::clamp(vol, 0.0f, 2.0f);
}

std::shared_ptr<SampleData> Sampler::loadSampleFile(const std::string& samplesDir, const std::string& filename) const {
    std::string filepath = samplesDir + "/" + filename;
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
    size_t dataByteOffset = 0;
    size_t totalDataBytes = 0;

    while (in) {
        ChunkHeader ch;
        if (!in.read(reinterpret_cast<char*>(&ch), sizeof(ch))) break;

        if (memcmp(ch.id, "fmt ", 4) == 0) {
            in.read(reinterpret_cast<char*>(&fmt), std::min<size_t>(ch.size, sizeof(fmt)));
            if (ch.size > sizeof(fmt)) in.seekg(ch.size - sizeof(fmt), std::ios::cur);
            hasFmt = true;
        } else if (memcmp(ch.id, "data", 4) == 0) {
            dataByteOffset = in.tellg();
            totalDataBytes = ch.size;
            break;
        } else {
            in.seekg(ch.size, std::ios::cur);
        }
    }

    if (!hasFmt || totalDataBytes == 0) return nullptr;

    auto sample = std::make_shared<SampleData>();
    sample->filename = filename;
    sample->fullPath = filepath;
    sample->sampleRate = fmt.sample_rate;
    sample->channels = fmt.num_channels;
    sample->dataByteOffset = dataByteOffset;

    size_t bytesPerFrame = fmt.num_channels * sizeof(int16_t);
    size_t totalFrames = (bytesPerFrame > 0) ? (totalDataBytes / bytesPerFrame) : 0;
    sample->numFrames = totalFrames;

    // DFD Architecture: Preload only the attack transient into RAM (~64KB), unless this
    // sampler keeps whole samples resident (looping drones need random access for the crossfade)
    if (m_fullyPreload || totalFrames <= DFD_PRELOAD_FRAMES) {
        pcmData.resize(totalFrames * fmt.num_channels);
        in.read(reinterpret_cast<char*>(pcmData.data()), totalFrames * bytesPerFrame);
        sample->isFullyPreloaded = true;
        sample->preloadFrames = totalFrames;
    } else {
        size_t preloadBytes = DFD_PRELOAD_FRAMES * bytesPerFrame;
        pcmData.resize(preloadBytes / sizeof(int16_t));
        in.read(reinterpret_cast<char*>(pcmData.data()), preloadBytes);
        sample->isFullyPreloaded = false;
        sample->preloadFrames = DFD_PRELOAD_FRAMES;
    }
    sample->pcm = std::move(pcmData);
    return sample;
}

bool Sampler::loadInstrument(const std::string& jsonPath, const std::string& samplesDir) {
    std::ifstream in(jsonPath);
    if (!in) {
        std::cerr << "[Sampler] Error: Cannot open instrument definition: " << jsonPath << std::endl;
        return false;
    }

    auto inst = std::make_unique<InstrumentData>();
    std::string line;
    int currentNote = -1;
    inst->name = "Stage Piano";

    while (std::getline(in, line)) {
        if (line.find("\"name\":") != std::string::npos) {
            size_t start = line.find('"', line.find(':')) + 1;
            size_t end = line.rfind('"');
            if (start < end) inst->name = line.substr(start, end - start);
        } else if (line.find("\"") != std::string::npos && line.find("\": [") != std::string::npos) {
            size_t q1 = line.find('"');
            size_t q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                currentNote = std::stoi(line.substr(q1 + 1, q2 - q1 - 1));
                inst->zones[currentNote].noteNumber = currentNote;
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

            inst->zones[currentNote].layers.push_back(layer);
        }
    }

    // Preload every layer now, on the loader thread, so noteOn never touches the disk
    size_t missing = 0;
    for (auto& [note, zone] : inst->zones) {
        for (auto& layer : zone.layers) {
            auto it = inst->sampleCache.find(layer.filename);
            if (it == inst->sampleCache.end()) {
                it = inst->sampleCache.emplace(layer.filename, loadSampleFile(samplesDir, layer.filename)).first;
            }
            layer.sample = it->second;
            if (!layer.sample) missing++;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_presetMutex);
        m_instrumentName = inst->name;
        m_preset.baseInstrument = inst->name;
        m_samplesDir = samplesDir;
    }

    std::cout << "[Sampler] Loaded instrument '" << inst->name << "' ("
              << inst->zones.size() << " note zones, " << inst->sampleCache.size() << " samples preloaded"
              << (missing ? ", " + std::to_string(missing) + " missing" : std::string()) << ")" << std::endl;

    // Publish: the pending slot holds one reference until the audio thread adopts it
    InstrumentData* raw = inst.get();
    raw->refs.store(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(m_ownedMutex);
        m_owned.push_back(std::move(inst));
    }
    if (InstrumentData* replaced = m_pending.exchange(raw, std::memory_order_acq_rel)) {
        replaced->refs.fetch_sub(1, std::memory_order_release); // never adopted
    }
    return true;
}

void Sampler::adoptPendingInstrument() {
    InstrumentData* next = m_pending.exchange(nullptr, std::memory_order_acq_rel);
    if (!next) return;
    if (m_current) {
        m_current->refs.fetch_sub(1, std::memory_order_release);
    }
    m_current = next; // takes over the pending reference
}

StreamSlot* Sampler::getStreamSlots(size_t& count) {
    count = m_voices.size();
    return m_slots.get();
}

void Sampler::collectGarbage() {
    std::lock_guard<std::mutex> lock(m_ownedMutex);
    m_owned.erase(std::remove_if(m_owned.begin(), m_owned.end(), [](const std::unique_ptr<InstrumentData>& inst) {
        return inst->refs.load(std::memory_order_acquire) == 0;
    }), m_owned.end());
}

const VelocityLayer* Sampler::findBestLayer(const InstrumentData& inst, int note, int velocity, int& outRootNote) const {
    if (inst.zones.empty()) return nullptr;

    auto it = inst.zones.find(note);
    if (it == inst.zones.end()) {
        int bestDist = 1000;
        int bestNote = -1;
        for (const auto& [n, zone] : inst.zones) {
            int d = std::abs(n - note);
            if (d < bestDist) {
                bestDist = d;
                bestNote = n;
            }
        }
        if (bestNote == -1) return nullptr;
        it = inst.zones.find(bestNote);
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

void Sampler::stopVoice(Voice& v, size_t idx) {
    v.active = false;
    v.stolen = false;
    v.underrun = false;
    // New epoch: any in-flight disk write for the old stream is discarded
    v.ringBuffer->reset();
    int expected = StreamSlot::Requested;
    m_slots[idx].state.compare_exchange_strong(expected, StreamSlot::Idle, std::memory_order_acq_rel);
    if (v.owner) {
        v.owner->refs.fetch_sub(1, std::memory_order_release);
        v.owner = nullptr;
    }
    v.sample = nullptr;
}

void Sampler::requestStream(Voice& v, size_t idx) {
    const SampleData* s = v.sample;
    if (!s || s->isFullyPreloaded || v.nextDiskFrame >= s->numFrames) return;

    StreamSlot& slot = m_slots[idx];
    if (slot.state.load(std::memory_order_acquire) != StreamSlot::Idle) return;

    // The last chunk for this stream came back short (unreadable file, truncated WAV): stop streaming
    // so the voice fades out when its buffered audio runs out
    if (slot.failedEpoch.load(std::memory_order_relaxed) == v.ringBuffer->getEpoch()) {
        v.nextDiskFrame = s->numFrames;
        return;
    }

    size_t avail = v.ringBuffer->getAvailableFrames();
    if (avail >= 32768) return;

    size_t framesToRead = std::min(DFD_CHUNK_FRAMES, s->numFrames - v.nextDiskFrame);
    double bufferedEnd = static_cast<double>(v.ringBufferHeadFrame + avail);
    double runwayFrames = std::max(0.0, bufferedEnd - v.position);
    double framesPerSec = std::max(1.0, v.playbackRate * m_sampleRate);

    slot.filePath = &s->fullPath;
    slot.fileByteOffset = s->dataByteOffset + v.nextDiskFrame * s->channels * sizeof(int16_t);
    slot.framesToRead = framesToRead;
    slot.channels = s->channels;
    slot.epoch = v.ringBuffer->getEpoch();
    slot.deadlineNs.store(DiskStreamer::nowNs() + static_cast<int64_t>(runwayFrames / framesPerSec * 1e9),
                          std::memory_order_relaxed);
    slot.targetBuffer = v.ringBuffer.get();
    slot.state.store(StreamSlot::Requested, std::memory_order_release);

    v.nextDiskFrame += framesToRead;
}

int Sampler::pickStealVictim() const {
    int best = -1;
    int bestPriority = 0;
    float bestLevel = 0.0f;
    uint64_t bestAge = 0;
    for (size_t i = 0; i < m_voices.size(); ++i) {
        const Voice& v = m_voices[i];
        if (!v.active || v.stolen) continue;
        int priority = v.isLooping ? 3 : (v.stage == Voice::Release ? 0 : (v.sustained ? 1 : 2));
        float level = v.envLevel * v.gain;
        bool better = best == -1 || priority < bestPriority ||
                      (priority == bestPriority && (level < bestLevel || (level == bestLevel && v.age < bestAge)));
        if (better) {
            best = static_cast<int>(i);
            bestPriority = priority;
            bestLevel = level;
            bestAge = v.age;
        }
    }
    return best;
}

void Sampler::noteOn(int note, int velocity, bool looping) {
    if (velocity <= 0) {
        noteOff(note);
        return;
    }
    // Events are applied before render(): pick up an instrument published since the last block
    adoptPendingInstrument();
    if (!m_current) return;

    int rootNote = note;
    const VelocityLayer* layer = findBestLayer(*m_current, note, velocity, rootNote);
    if (!layer || !layer->sample) return;
    const SampleData* sample = layer->sample.get();

    // Retrigger check: if the same note is already sounding, fade it out smoothly
    for (auto& oldV : m_voices) {
        if (oldV.active && oldV.note == note) {
            oldV.stage = Voice::Release;
            oldV.sustained = false;
            oldV.releaseStep = std::max(oldV.releaseStep, 1.0f / (m_sampleRate * 0.020f));
        }
    }

    // Find a free voice; past the polyphony limit, fade the least important voice out (5ms)
    int voiceIdx = -1;
    size_t activeCount = 0;
    for (size_t i = 0; i < m_voices.size(); ++i) {
        if (m_voices[i].active) {
            if (!m_voices[i].stolen) activeCount++;
        } else if (voiceIdx == -1) {
            voiceIdx = static_cast<int>(i);
        }
    }

    if (activeCount >= m_maxPolyphony) {
        int victim = pickStealVictim();
        if (victim >= 0) {
            Voice& sv = m_voices[victim];
            sv.stolen = true;
            sv.stage = Voice::Release;
            sv.releaseStep = std::max(sv.releaseStep, 1.0f / (m_sampleRate * 0.005f));
        }
    }

    if (voiceIdx == -1) {
        // Reserve exhausted: hard steal the quietest voice (last resort)
        int victim = pickStealVictim();
        if (victim < 0) {
            float minLevel = 2.0f;
            for (size_t i = 0; i < m_voices.size(); ++i) {
                if (m_voices[i].envLevel < minLevel) {
                    minLevel = m_voices[i].envLevel;
                    victim = static_cast<int>(i);
                }
            }
        }
        if (victim < 0) return;
        stopVoice(m_voices[victim], static_cast<size_t>(victim));
        voiceIdx = victim;
    }

    Voice& v = m_voices[voiceIdx];

    v.ringBuffer->reset();
    v.active = true;
    v.stolen = false;
    v.underrun = false;
    v.isLooping = looping;
    v.note = note;
    v.velocity = velocity;
    v.sample = sample;
    v.owner = m_current;
    v.owner->refs.fetch_add(1, std::memory_order_relaxed);
    v.position = 0.0;
    v.lastL = v.lastR = 0.0f;

    v.nextDiskFrame = sample->preloadFrames;
    v.ringBufferHeadFrame = sample->preloadFrames;

    v.playbackRate = std::pow(2.0, (note - rootNote) / 12.0) *
                     (static_cast<double>(sample->sampleRate) / m_sampleRate);
    float normVel = static_cast<float>(velocity) / 127.0f;
    v.gain = std::pow(normVel, 1.3f);

    // Envelope configuration
    float attackMs = m_attackMs.load();
    if (attackMs <= 2.0f) {
        v.stage = Voice::Decay;
        v.envLevel = 1.0f;
    } else {
        v.stage = Voice::Attack;
        v.envLevel = 0.0f;
        v.attackStep = 1.0f / (attackMs * 0.001f * m_sampleRate);
    }

    float decaySamples = std::max(10.0f, m_decayMs.load() * 0.001f * m_sampleRate);
    v.sustainLevel = m_sustainLevel.load();
    v.decayStep = (1.0f - v.sustainLevel) / decaySamples;

    float releaseSamples = std::max(10.0f, m_releaseMs.load() * 0.001f * m_sampleRate);
    v.releaseStep = 1.0f / releaseSamples;

    v.sustained = false;
    v.age = ++m_voiceAgeCounter;

    // Configure Per-Voice Resonant Cutoff Filter
    float q = 0.707f / std::max(0.05f, (1.0f - m_filterResonance.load() * 0.95f));
    v.filter[0].configure(FilterType::Lowpass, m_sampleRate, m_filterCutoff.load(), q);
    v.filter[1].configure(FilterType::Lowpass, m_sampleRate, m_filterCutoff.load(), q);
    v.filter[0].reset();
    v.filter[1].reset();

    // Initial DFD Chunk Schedule: Queue first disk block while attack plays from RAM
    requestStream(v, static_cast<size_t>(voiceIdx));
}

void Sampler::noteOff(int note) {
    for (auto& v : m_voices) {
        if (v.active && v.note == note && !v.stolen) {
            if (m_sustainPedal) {
                v.sustained = true;
            } else {
                v.stage = Voice::Release;
            }
        }
    }
}

void Sampler::setSustainPedal(bool pressed) {
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

void Sampler::releaseAll(float fadeMs) {
    float step = 1.0f / std::max(1.0f, m_sampleRate * fadeMs * 0.001f);
    for (auto& v : m_voices) {
        if (v.active) {
            v.sustained = false;
            v.stage = Voice::Release;
            v.releaseStep = std::max(v.releaseStep, step);
        }
    }
}

void Sampler::allNotesOff() {
    for (size_t i = 0; i < m_voices.size(); ++i) {
        if (m_voices[i].active) stopVoice(m_voices[i], i);
    }
    m_activeVoices.store(0, std::memory_order_relaxed);
}

void Sampler::render(float* outputBuffer, uint32_t numFrames, uint32_t numChannels) {
    adoptPendingInstrument();

    const float inv32768 = 1.0f / 32768.0f;
    const float filterCutoff = m_filterCutoff.load();
    const bool useFilter = (filterCutoff < 19000.0f || m_filterResonance.load() > 0.05f);
    const float masterVolume = m_masterVolume.load();
    int activeCount = 0;

    for (size_t vi = 0; vi < m_voices.size(); ++vi) {
        Voice& v = m_voices[vi];
        if (!v.active || !v.sample) continue;

        const SampleData* s = v.sample;
        const int16_t* pcm = s->pcm.data();
        size_t sampleFrames = s->numFrames;
        size_t preloadFrames = s->preloadFrames;
        uint16_t sampleCh = s->channels;

        // Crossfade loop region for ambient drones (needs the whole sample in RAM). Samples that
        // cannot loop play through and stop at their end like any other voice.
        const bool canLoop = v.isLooping && s->isFullyPreloaded && sampleFrames >= 64;
        const size_t loopStart = static_cast<size_t>(sampleFrames * 0.15);
        const size_t loopEnd = static_cast<size_t>(sampleFrames * 0.85);
        const size_t xfadeLen = canLoop
            ? std::max<size_t>(1, std::min<size_t>(static_cast<size_t>(m_sampleRate * 2.0f), (loopEnd - loopStart) / 3))
            : 1;

        for (uint32_t f = 0; f < numFrames; ++f) {
            // The crossfade ends fully on loopStart + xfadeLen, so the wrap lands there (keeping
            // the fractional overshoot) and the seam is continuous
            if (canLoop && v.position >= loopEnd) {
                v.position = static_cast<double>(loopStart + xfadeLen) + (v.position - static_cast<double>(loopEnd));
            }

            if (!canLoop && v.position >= sampleFrames - 1) {
                stopVoice(v, vi);
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
                        stopVoice(v, vi);
                    }
                    break;
                case Voice::Off:
                case Voice::Idle:
                    stopVoice(v, vi);
                    break;
            }

            if (!v.active) break;

            size_t idx0 = static_cast<size_t>(v.position);
            size_t idx1 = std::min(idx0 + 1, sampleFrames - 1);
            float frac = static_cast<float>(v.position - idx0);

            float sL = v.lastL, sR = v.lastR;
            bool fetched = false;

            if (v.underrun) {
                // Hold the last sample while the 15ms fade runs: no step to zero
            } else if (s->isFullyPreloaded || idx1 < preloadFrames) {
                // 100% in RAM Preload
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
                fetched = true;
            } else if (idx0 == preloadFrames - 1 && idx1 == preloadFrames) {
                // Boundary crossing sample between preload and ring buffer
                int16_t b0_L = 0, b0_R = 0;
                if (sampleCh == 2) {
                    b0_L = pcm[idx0 * 2];
                    b0_R = pcm[idx0 * 2 + 1];
                } else {
                    b0_L = b0_R = pcm[idx0];
                }

                int16_t b1_L = 0, b1_R = 0;
                float s0_L = static_cast<float>(b0_L) * inv32768;
                float s0_R = static_cast<float>(b0_R) * inv32768;
                if (v.ringBuffer->peekFrame(0, b1_L, b1_R)) {
                    float s1_L = static_cast<float>(b1_L) * inv32768;
                    float s1_R = static_cast<float>(b1_R) * inv32768;
                    sL = s0_L + frac * (s1_L - s0_L);
                    sR = s0_R + frac * (s1_R - s0_R);
                } else {
                    // Preload boundary underrun protection
                    sL = s0_L;
                    sR = s0_R;
                }
                fetched = true;
            } else if (idx0 >= v.ringBufferHeadFrame) {
                // Streaming from Ring Buffer
                size_t relOffset0 = idx0 - v.ringBufferHeadFrame;
                size_t relOffset1 = idx1 - v.ringBufferHeadFrame;

                if (relOffset1 < v.ringBuffer->getAvailableFrames()) {
                    int16_t r0_L = 0, r0_R = 0, r1_L = 0, r1_R = 0;
                    v.ringBuffer->peekFrame(relOffset0, r0_L, r0_R);
                    v.ringBuffer->peekFrame(relOffset1, r1_L, r1_R);

                    float s0_L = static_cast<float>(r0_L) * inv32768;
                    float s1_L = static_cast<float>(r1_L) * inv32768;
                    float s0_R = static_cast<float>(r0_R) * inv32768;
                    float s1_R = static_cast<float>(r1_R) * inv32768;

                    sL = s0_L + frac * (s1_L - s0_L);
                    sR = s0_R + frac * (s1_R - s0_R);
                    fetched = true;

                    // Advance ring buffer once consumer is well ahead of head
                    if (relOffset0 > 2048) {
                        size_t adv = relOffset0 - 512;
                        v.ringBuffer->advanceFrames(adv);
                        v.ringBufferHeadFrame += adv;
                    }
                }
            }

            if (fetched) {
                v.lastL = sL;
                v.lastR = sR;
            } else if (!v.underrun) {
                // DFDError: Buffer Underrun -> Graceful Micro-fadeout on the held sample
                DiskStreamer::getInstance().recordUnderrun();
                v.underrun = true;
                v.stage = Voice::Release;
                v.releaseStep = std::max(v.releaseStep, 1.0f / (m_sampleRate * 0.015f)); // 15ms
            }

            // Continuous seamless crossfade loop for ambient drones
            if (canLoop) {
                if (v.position >= loopEnd - xfadeLen && v.position < loopEnd) {
                    float xfadeAlpha = static_cast<float>(v.position - (loopEnd - xfadeLen)) / xfadeLen;
                    size_t wrapPos = loopStart + static_cast<size_t>(v.position - (loopEnd - xfadeLen));
                    if (wrapPos + 1 < preloadFrames) {
                        float w0 = static_cast<float>(pcm[wrapPos * sampleCh]) * inv32768;
                        float w1 = static_cast<float>(pcm[(wrapPos + 1) * sampleCh]) * inv32768;
                        float wrapSampleL = w0 + frac * (w1 - w0);

                        float wrapSampleR = wrapSampleL;
                        if (sampleCh == 2) {
                            float wr0 = static_cast<float>(pcm[wrapPos * 2 + 1]) * inv32768;
                            float wr1 = static_cast<float>(pcm[(wrapPos + 1) * 2 + 1]) * inv32768;
                            wrapSampleR = wr0 + frac * (wr1 - wr0);
                        }

                        sL = sL * (1.0f - xfadeAlpha) + wrapSampleL * xfadeAlpha;
                        sR = sR * (1.0f - xfadeAlpha) + wrapSampleR * xfadeAlpha;
                    }
                }
            }

            // Apply Per-Voice Dynamic Resonant Cutoff Filter
            if (useFilter) {
                sL = v.filter[0].process(sL);
                sR = v.filter[1].process(sR);
            }

            // Apply envelope, gain & pan
            float amp = v.envLevel * v.gain * masterVolume;
            sL *= amp;
            sR *= amp;

            outputBuffer[f * numChannels] += sL;
            if (numChannels > 1) {
                outputBuffer[f * numChannels + 1] += sR;
            }

            v.position += v.playbackRate;
        }

        // DFD Ring Buffer Replenishment Check
        if (v.active) {
            activeCount++;
            requestStream(v, vi);
        }
    }

    m_activeVoices.store(activeCount, std::memory_order_relaxed);

    // Dynamic Processing Chain: Equalizer -> Compressor
    if (m_eqEnabled.load(std::memory_order_relaxed)) {
        m_equalizer.process(outputBuffer, numFrames, numChannels);
    }

    if (m_compEnabled.load(std::memory_order_relaxed)) {
        m_compressor.process(outputBuffer, numFrames, numChannels);
    }
}
