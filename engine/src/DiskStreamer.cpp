#include "DiskStreamer.hpp"
#include <chrono>
#include <iostream>

DiskStreamer::DiskStreamer() {
    start();
}

void DiskStreamer::start() {
    if (m_running.load()) return;
    m_running.store(true);
    m_workerThread = std::thread(&DiskStreamer::workerLoop, this);
}

void DiskStreamer::stop() {
    if (!m_running.load()) return;
    m_running.store(false);
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_fileCache.clear();
}

void DiskStreamer::registerClient(StreamClient* client) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clients.push_back(client);
}

void DiskStreamer::unregisterClient(StreamClient* client) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), client), m_clients.end());
}

int64_t DiskStreamer::nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::ifstream* DiskStreamer::openCached(const std::string& path) {
    auto it = m_fileCache.find(path);
    if (it == m_fileCache.end()) {
        if (m_fileCache.size() >= kMaxOpenFiles) {
            auto oldest = std::min_element(m_fileCache.begin(), m_fileCache.end(),
                [](const auto& a, const auto& b) { return a.second.lastUse < b.second.lastUse; });
            m_fileCache.erase(oldest);
        }
        auto stream = std::make_unique<std::ifstream>(path, std::ios::binary);
        if (!stream->is_open()) return nullptr;
        it = m_fileCache.emplace(path, CachedFile{std::move(stream), 0}).first;
    }
    it->second.lastUse = ++m_useCounter;
    return it->second.stream.get();
}

void DiskStreamer::workerLoop() {
    std::vector<int16_t> scratchBuffer(65536 * 2); // 64K frames stereo buffer
    auto lastMetricTime = std::chrono::steady_clock::now();
    auto lastGcTime = lastMetricTime;
    std::chrono::microseconds activeDuration(0);

    while (m_running.load()) {
        bool didWork = false;
        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);

            // Earliest Deadline First across every registered voice slot
            StreamSlot* best = nullptr;
            int pending = 0;
            for (StreamClient* client : m_clients) {
                size_t count = 0;
                StreamSlot* slots = client->getStreamSlots(count);
                for (size_t i = 0; i < count; ++i) {
                    if (slots[i].state.load(std::memory_order_acquire) != StreamSlot::Requested) continue;
                    pending++;
                    if (!best || slots[i].deadlineNs.load(std::memory_order_relaxed) <
                                 best->deadlineNs.load(std::memory_order_relaxed)) {
                        best = &slots[i];
                    }
                }
            }
            m_activeRequests.store(pending, std::memory_order_relaxed);

            int expected = StreamSlot::Requested;
            if (best && best->state.compare_exchange_strong(expected, StreamSlot::InFlight, std::memory_order_acq_rel)) {
                auto workStart = std::chrono::steady_clock::now();

                // A voice that ended or was stolen bumped its ring epoch: skip without touching its sample
                if (best->targetBuffer && best->targetBuffer->getEpoch() == best->epoch && best->filePath) {
                    size_t framesWritten = 0;
                    if (std::ifstream* stream = openCached(*best->filePath)) {
                        stream->clear();
                        stream->seekg(static_cast<std::streamoff>(best->fileByteOffset), std::ios::beg);

                        size_t samplesToRead = best->framesToRead * best->channels;
                        if (scratchBuffer.size() < samplesToRead) {
                            scratchBuffer.resize(samplesToRead);
                        }
                        stream->read(reinterpret_cast<char*>(scratchBuffer.data()), samplesToRead * sizeof(int16_t));
                        size_t framesRead = static_cast<size_t>(stream->gcount()) / (best->channels * sizeof(int16_t));
                        if (framesRead > 0) {
                            framesWritten = best->targetBuffer->writeFrames(scratchBuffer.data(), framesRead, best->epoch);
                        }
                    }
                    // Only report for the stream still playing: a stolen voice's read is just discarded
                    if (framesWritten < best->framesToRead && best->targetBuffer->getEpoch() == best->epoch) {
                        best->failedEpoch.store(best->epoch, std::memory_order_relaxed);
                    }
                }

                best->state.store(StreamSlot::Idle, std::memory_order_release);
                didWork = true;
                activeDuration += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - workStart);
            }

            // Free instruments that are no longer referenced. Runs here, between reads, so a sample
            // can never be freed while this thread is streaming from it.
            auto now = std::chrono::steady_clock::now();
            if (now - lastGcTime >= std::chrono::milliseconds(100)) {
                for (StreamClient* client : m_clients) client->collectGarbage();
                lastGcTime = now;
            }
        }

        // Update load percentage metric every 250ms
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastMetricTime);
        if (elapsed.count() >= 250000) {
            int load = static_cast<int>((activeDuration.count() * 100) / elapsed.count());
            m_loadPercent.store(std::clamp(load, 0, 100), std::memory_order_relaxed);
            activeDuration = std::chrono::microseconds(0);
            lastMetricTime = now;
        }

        if (!didWork) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
