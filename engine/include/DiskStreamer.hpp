#pragma once

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <fstream>
#include <cstdint>
#include <algorithm>

// Single-Producer Single-Consumer (SPSC) Lock-Free Circular Buffer for Interleaved Stereo PCM
class VoiceRingBuffer {
public:
    explicit VoiceRingBuffer(size_t capacityFrames = 65536, uint16_t channels = 2)
        : m_channels(channels),
          m_capacityFrames(capacityFrames),
          m_buffer(capacityFrames * channels, 0),
          m_writeFrame(0),
          m_readFrame(0) {}

    void reset() {
        m_writeFrame.store(0, std::memory_order_relaxed);
        m_readFrame.store(0, std::memory_order_relaxed);
    }

    size_t getCapacityFrames() const { return m_capacityFrames; }
    uint16_t getChannels() const { return m_channels; }

    size_t getAvailableFrames() const {
        size_t w = m_writeFrame.load(std::memory_order_acquire);
        size_t r = m_readFrame.load(std::memory_order_relaxed);
        return (w >= r) ? (w - r) : (m_capacityFrames - (r - w));
    }

    size_t getFreeFrames() const {
        return m_capacityFrames - 1 - getAvailableFrames();
    }

    // Called by Background I/O Thread (Producer)
    size_t writeFrames(const int16_t* source, size_t frames) {
        size_t freeSpace = getFreeFrames();
        size_t toWrite = std::min(frames, freeSpace);
        if (toWrite == 0) return 0;

        size_t w = m_writeFrame.load(std::memory_order_relaxed);
        size_t firstChunk = std::min(toWrite, m_capacityFrames - w);
        size_t secondChunk = toWrite - firstChunk;

        std::copy_n(source, firstChunk * m_channels, m_buffer.data() + w * m_channels);
        if (secondChunk > 0) {
            std::copy_n(source + firstChunk * m_channels, secondChunk * m_channels, m_buffer.data());
        }

        size_t newW = (w + toWrite) % m_capacityFrames;
        m_writeFrame.store(newW, std::memory_order_release);
        return toWrite;
    }

    // Called by Audio DSP Thread (Consumer)
    size_t readFrames(int16_t* dest, size_t frames) {
        size_t avail = getAvailableFrames();
        size_t toRead = std::min(frames, avail);
        if (toRead == 0) return 0;

        size_t r = m_readFrame.load(std::memory_order_relaxed);
        size_t firstChunk = std::min(toRead, m_capacityFrames - r);
        size_t secondChunk = toRead - firstChunk;

        std::copy_n(m_buffer.data() + r * m_channels, firstChunk * m_channels, dest);
        if (secondChunk > 0) {
            std::copy_n(m_buffer.data(), secondChunk * m_channels, dest + firstChunk * m_channels);
        }

        size_t newR = (r + toRead) % m_capacityFrames;
        m_readFrame.store(newR, std::memory_order_release);
        return toRead;
    }

    // Peek frame at relative offset from read pointer for sub-sample interpolation
    bool peekFrame(size_t relativeFrameOffset, int16_t& outL, int16_t& outR) const {
        size_t avail = getAvailableFrames();
        if (relativeFrameOffset >= avail) return false;

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
        size_t avail = getAvailableFrames();
        size_t toAdvance = std::min(frames, avail);
        size_t r = m_readFrame.load(std::memory_order_relaxed);
        size_t newR = (r + toAdvance) % m_capacityFrames;
        m_readFrame.store(newR, std::memory_order_release);
    }

private:
    uint16_t m_channels;
    size_t m_capacityFrames;
    std::vector<int16_t> m_buffer;
    std::atomic<size_t> m_writeFrame;
    std::atomic<size_t> m_readFrame;
};

// Streaming Request Descriptor for Earliest Deadline First (EDF) Scheduling
struct DFDStreamRequest {
    uint32_t voiceId = 0;
    std::string filePath;
    size_t fileByteOffset = 0;
    size_t framesToRead = 0;
    uint16_t channels = 2;
    double deadlineSec = 0.0; // Urgent deadline for EDF scheduler
    std::shared_ptr<std::atomic<bool>> cancelToken;
    VoiceRingBuffer* targetBuffer = nullptr;
    std::atomic<bool>* inFlightFlag = nullptr;
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

    // Enqueue a read request from Audio DSP thread (Non-blocking)
    bool enqueueRequest(const DFDStreamRequest& req);

    // Cancel all pending requests for a given voice ID
    void cancelRequestsForVoice(uint32_t voiceId);

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

    std::atomic<bool> m_running{false};
    std::thread m_workerThread;

    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::vector<DFDStreamRequest> m_requests;

    // Cached open file streams to eliminate filesystem open() syscall overhead during playback
    std::unordered_map<std::string, std::unique_ptr<std::ifstream>> m_fileCache;

    // Performance metrics
    std::atomic<int> m_loadPercent{0};
    std::atomic<int> m_activeRequests{0};
    std::atomic<uint64_t> m_underruns{0};
};
