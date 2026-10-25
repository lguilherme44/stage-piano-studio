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
    m_cv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_requests.clear();
    m_fileCache.clear();
}

bool DiskStreamer::enqueueRequest(const DFDStreamRequest& req) {
    if (!m_running.load()) return false;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_requests.push_back(req);
        m_activeRequests.store(static_cast<int>(m_requests.size()), std::memory_order_relaxed);
    }
    m_cv.notify_one();
    return true;
}

void DiskStreamer::cancelRequestsForVoice(uint32_t voiceId) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    for (auto& r : m_requests) {
        if (r.voiceId == voiceId && r.cancelToken) {
            r.cancelToken->store(true, std::memory_order_relaxed);
        }
    }
}

void DiskStreamer::workerLoop() {
    std::vector<int16_t> scratchBuffer(65536 * 2); // 64K frames stereo buffer
    auto lastMetricTime = std::chrono::steady_clock::now();
    std::chrono::microseconds activeDuration(0);

    while (m_running.load()) {
        DFDStreamRequest currentReq{};
        bool hasRequest = false;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(20), [&] {
                return !m_running.load() || !m_requests.empty();
            });

            if (!m_running.load()) break;

            // Remove canceled requests
            m_requests.erase(
                std::remove_if(m_requests.begin(), m_requests.end(), [](const DFDStreamRequest& r) {
                    if (r.cancelToken && r.cancelToken->load(std::memory_order_relaxed)) {
                        if (r.inFlightFlag) r.inFlightFlag->store(false, std::memory_order_relaxed);
                        return true;
                    }
                    return false;
                }),
                m_requests.end()
            );

            if (!m_requests.empty()) {
                // Earliest Deadline First (EDF) scheduler: Sort by deadline
                auto bestIt = std::min_element(m_requests.begin(), m_requests.end(),
                    [](const DFDStreamRequest& a, const DFDStreamRequest& b) {
                        return a.deadlineSec < b.deadlineSec;
                    });

                currentReq = *bestIt;
                m_requests.erase(bestIt);
                hasRequest = true;
            }
            m_activeRequests.store(static_cast<int>(m_requests.size()), std::memory_order_relaxed);
        }

        if (hasRequest) {
            auto workStart = std::chrono::steady_clock::now();

            if (!currentReq.cancelToken || !currentReq.cancelToken->load(std::memory_order_relaxed)) {
                // Check or open cached file stream
                auto it = m_fileCache.find(currentReq.filePath);
                if (it == m_fileCache.end()) {
                    auto fileStream = std::make_unique<std::ifstream>(currentReq.filePath, std::ios::binary);
                    if (fileStream && fileStream->is_open()) {
                        it = m_fileCache.emplace(currentReq.filePath, std::move(fileStream)).first;
                    }
                }

                if (it != m_fileCache.end() && it->second && it->second->is_open()) {
                    auto& stream = *(it->second);
                    stream.clear();
                    stream.seekg(currentReq.fileByteOffset, std::ios::beg);

                    size_t samplesToRead = currentReq.framesToRead * currentReq.channels;
                    if (scratchBuffer.size() < samplesToRead) {
                        scratchBuffer.resize(samplesToRead);
                    }

                    stream.read(reinterpret_cast<char*>(scratchBuffer.data()), samplesToRead * sizeof(int16_t));
                    std::streamsize bytesRead = stream.gcount();
                    size_t framesRead = static_cast<size_t>(bytesRead) / (currentReq.channels * sizeof(int16_t));

                    if (framesRead > 0 && currentReq.targetBuffer) {
                        if (!currentReq.cancelToken || !currentReq.cancelToken->load(std::memory_order_relaxed)) {
                            currentReq.targetBuffer->writeFrames(scratchBuffer.data(), framesRead);
                        }
                    }
                }
            }

            if (currentReq.inFlightFlag) {
                currentReq.inFlightFlag->store(false, std::memory_order_release);
            }

            auto workEnd = std::chrono::steady_clock::now();
            activeDuration += std::chrono::duration_cast<std::chrono::microseconds>(workEnd - workStart);
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
    }
}
