#pragma once

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <fstream>
#include <cstdint>
#include <algorithm>

// Single-Producer Single-Consumer (SPSC) Lock-Free Circular Buffer for Interleaved Stereo PCM.
// The write index is packed with an epoch: reset() (consumer) bumps the epoch, and a producer
// write tagged with an older epoch is discarded instead of corrupting the new stream.
class VoiceRingBuffer {
public:
    explicit VoiceRingBuffer(size_t capacityFrames = 65536, uint16_t channels = 2)
        : m_channels(channels),
          m_capacityFrames(capacityFrames),
          m_buffer(capacityFrames * channels, 0) {}

    // Called by Audio DSP Thread (Consumer). Returns the new epoch.
    uint32_t reset() {
        uint32_t epoch = epochOf(m_write.load(std::memory_order_relaxed)) + 1;
        m_readFrame.store(0, std::memory_order_relaxed);
        m_write.store(pack(epoch, 0), std::memory_order_seq_cst);
        return epoch;
    }

    uint32_t getEpoch() const { return epochOf(m_write.load(std::memory_order_acquire)); }
    size_t getCapacityFrames() const { return m_capacityFrames; }
    uint16_t getChannels() const { return m_channels; }

    size_t getAvailableFrames() const {
        size_t w = frameOf(m_write.load(std::memory_order_acquire));
        size_t r = m_readFrame.load(std::memory_order_acquire);
        return (w >= r) ? (w - r) : (m_capacityFrames - (r - w));
    }

    // Called by Background I/O Thread (Producer). Writes nothing if the epoch moved on.
    size_t writeFrames(const int16_t* source, size_t frames, uint32_t epoch) {
        uint64_t packed = m_write.load(std::memory_order_acquire);
        if (epochOf(packed) != epoch) return 0;

        size_t w = frameOf(packed);
        size_t r = m_readFrame.load(std::memory_order_acquire);
        size_t avail = (w >= r) ? (w - r) : (m_capacityFrames - (r - w));
        size_t toWrite = std::min(frames, m_capacityFrames - 1 - avail);
        if (toWrite == 0) return 0;

        size_t firstChunk = std::min(toWrite, m_capacityFrames - w);
        size_t secondChunk = toWrite - firstChunk;
        std::copy_n(source, firstChunk * m_channels, m_buffer.data() + w * m_channels);
        if (secondChunk > 0) {
            std::copy_n(source + firstChunk * m_channels, secondChunk * m_channels, m_buffer.data());
        }

        uint64_t next = pack(epoch, (w + toWrite) % m_capacityFrames);
        if (!m_write.compare_exchange_strong(packed, next, std::memory_order_release, std::memory_order_relaxed)) {
            return 0; // consumer reset the buffer while we were copying
        }
        return toWrite;
    }

    // Peek frame at relative offset from read pointer for sub-sample interpolation
    bool peekFrame(size_t relativeFrameOffset, int16_t& outL, int16_t& outR) const {
        if (relativeFrameOffset >= getAvailableFrames()) return false;

        size_t r = m_readFrame.load(std::memory_order_relaxed);
        size_t actualIdx = (r + relativeFrameOffset) % m_capacityFrames;

        if (m_channels == 2) {
            outL = m_buffer[actualIdx * 2];
            outR = m_buffer[actualIdx * 2 + 1];
        } else {
            outL = outR = m_buffer[actualIdx];
        }
        return true;
    }

    void advanceFrames(size_t frames) {
        size_t toAdvance = std::min(frames, getAvailableFrames());
        size_t r = m_readFrame.load(std::memory_order_relaxed);
        m_readFrame.store((r + toAdvance) % m_capacityFrames, std::memory_order_release);
    }

private:
    static uint64_t pack(uint32_t epoch, size_t frame) { return (static_cast<uint64_t>(epoch) << 32) | static_cast<uint32_t>(frame); }
    static uint32_t epochOf(uint64_t packed) { return static_cast<uint32_t>(packed >> 32); }
    static size_t frameOf(uint64_t packed) { return static_cast<size_t>(packed & 0xFFFFFFFFu); }

    uint16_t m_channels;
    size_t m_capacityFrames;
    std::vector<int16_t> m_buffer;
    std::atomic<uint64_t> m_write{0};
    std::atomic<size_t> m_readFrame{0};
};

// One per voice. The audio thread fills the payload only while the slot is Idle, then
// publishes it as Requested; the I/O thread claims it (InFlight), reads, and returns it to Idle.
// No locks and no allocation on the audio side.
struct StreamSlot {
    enum State : int { Idle = 0, Requested = 1, InFlight = 2 };
    std::atomic<int> state{Idle};

    const std::string* filePath = nullptr; // owned by SampleData, kept alive by the instrument refcount
    size_t fileByteOffset = 0;
    size_t framesToRead = 0;
    uint16_t channels = 2;
    uint32_t epoch = 0;
    VoiceRingBuffer* targetBuffer = nullptr;

    // Read by the I/O thread while scanning (before it owns the slot), hence atomic
    std::atomic<int64_t> deadlineNs{0}; // absolute steady_clock deadline for EDF scheduling

    // Set by the I/O thread to the stream epoch whose read came back short or failed. The voice must
    // stop streaming: appending the next chunk would silently skip audio instead of fading out.
    std::atomic<uint32_t> failedEpoch{0};
};

// Implemented by Sampler: exposes its slots and frees retired instruments on the I/O thread.
class StreamClient {
public:
    virtual ~StreamClient() = default;
    virtual StreamSlot* getStreamSlots(size_t& count) = 0;
    virtual void collectGarbage() = 0;
};

class DiskStreamer {
public:
    static DiskStreamer& getInstance() {
        static DiskStreamer instance;
        return instance;
    }

    ~DiskStreamer() {
        stop();
    }

    void start();
    void stop();

    void registerClient(StreamClient* client);
    void unregisterClient(StreamClient* client);

    static int64_t nowNs();

    // Statistics & Diagnostics (Kontakt-aligned)
    int getDfdLoadPercent() const { return m_loadPercent.load(std::memory_order_relaxed); }
    int getActiveRequests() const { return m_activeRequests.load(std::memory_order_relaxed); }
    uint64_t getUnderrunCount() const { return m_underruns.load(std::memory_order_relaxed); }
    void recordUnderrun() { m_underruns.fetch_add(1, std::memory_order_relaxed); }

private:
    DiskStreamer();
    DiskStreamer(const DiskStreamer&) = delete;
    DiskStreamer& operator=(const DiskStreamer&) = delete;

    void workerLoop();
    std::ifstream* openCached(const std::string& path);

    std::atomic<bool> m_running{false};
    std::thread m_workerThread;

    std::mutex m_clientsMutex; // I/O thread + client construction/destruction only, never the audio thread
    std::vector<StreamClient*> m_clients;

    // Bounded LRU of open streams: the MSVC CRT caps open FILE streams at 512 by default,
    // and a single piano library has more sample files than that.
    struct CachedFile {
        std::unique_ptr<std::ifstream> stream;
        uint64_t lastUse = 0;
    };
    static constexpr size_t kMaxOpenFiles = 64;
    std::unordered_map<std::string, CachedFile> m_fileCache;
    uint64_t m_useCounter = 0;

    // Performance metrics
    std::atomic<int> m_loadPercent{0};
    std::atomic<int> m_activeRequests{0};
    std::atomic<uint64_t> m_underruns{0};
};
