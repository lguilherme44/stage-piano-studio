#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

// Bounded lock-free multi-producer queue (Dmitry Vyukov's MPMC algorithm).
// Producers: MIDI and UI threads. Consumer: the audio thread. Never allocates after construction.
template <typename T>
class EventQueue {
public:
    explicit EventQueue(size_t capacityPow2 = 2048)
        : m_mask(capacityPow2 - 1), m_cells(std::make_unique<Cell[]>(capacityPow2)) {
        for (size_t i = 0; i < capacityPow2; ++i) m_cells[i].sequence.store(i, std::memory_order_relaxed);
    }

    // Returns false when the queue is full (event dropped).
    bool push(const T& value) {
        size_t pos = m_enqueuePos.load(std::memory_order_relaxed);
        for (;;) {
            Cell& cell = m_cells[pos & m_mask];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (m_enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    cell.data = value;
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = m_enqueuePos.load(std::memory_order_relaxed);
            }
        }
    }

    bool pop(T& out) {
        size_t pos = m_dequeuePos.load(std::memory_order_relaxed);
        for (;;) {
            Cell& cell = m_cells[pos & m_mask];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            if (diff == 0) {
                if (m_dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    out = cell.data;
                    cell.sequence.store(pos + m_mask + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = m_dequeuePos.load(std::memory_order_relaxed);
            }
        }
    }

private:
    struct Cell {
        std::atomic<size_t> sequence{0};
        T data{};
    };

    const size_t m_mask;
    std::unique_ptr<Cell[]> m_cells;
    alignas(64) std::atomic<size_t> m_enqueuePos{0};
    alignas(64) std::atomic<size_t> m_dequeuePos{0};
};
