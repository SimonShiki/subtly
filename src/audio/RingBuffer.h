#pragma once
#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

// Single-producer, single-consumer lock-free ring buffer of floats.
// Producer: PipeWire realtime thread. Consumer: ASR worker thread.
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : m_buf(capacity + 1) {}

    // Return the number of samples written and drop excess samples when full.
    size_t push(std::span<const float> in) {
        const size_t n = m_buf.size();
        size_t w = m_write.load(std::memory_order_relaxed);
        const size_t r = m_read.load(std::memory_order_acquire);
        size_t freeSpace = (r + n - w - 1) % n;
        size_t count = std::min(freeSpace, in.size());
        for (size_t i = 0; i < count; ++i) {
            m_buf[w] = in[i];
            w = (w + 1 == n) ? 0 : w + 1;
        }
        m_write.store(w, std::memory_order_release);
        return count;
    }

    // Pop up to out.size() samples and return the count.
    size_t pop(std::span<float> out) {
        const size_t n = m_buf.size();
        size_t r = m_read.load(std::memory_order_relaxed);
        const size_t w = m_write.load(std::memory_order_acquire);
        size_t avail = (w + n - r) % n;
        size_t count = std::min(avail, out.size());
        for (size_t i = 0; i < count; ++i) {
            out[i] = m_buf[r];
            r = (r + 1 == n) ? 0 : r + 1;
        }
        m_read.store(r, std::memory_order_release);
        return count;
    }

    // Call only from the consumer thread.
    void clear() { m_read.store(m_write.load(std::memory_order_acquire), std::memory_order_release); }

private:
    std::vector<float> m_buf;
    alignas(64) std::atomic<size_t> m_write{0};
    alignas(64) std::atomic<size_t> m_read{0};
};
