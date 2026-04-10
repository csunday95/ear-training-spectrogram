#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace audio {

// Lock-free single-producer single-consumer ring buffer.
// Capacity is a compile-time power-of-2 template parameter; storage is std::array (no heap).
// Producer: audio callback thread (write). Consumer: main/GL thread (read/peek).
template<typename T, uint32_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static constexpr uint32_t kMask = Capacity - 1;

public:
    RingBuffer() = default;

    // Number of elements available to read.
    uint32_t available() const {
        return static_cast<uint32_t>(write_.load(std::memory_order_acquire) -
                                     read_.load(std::memory_order_relaxed));
    }

    constexpr uint32_t capacity() const { return Capacity; }

    // Write up to `count` elements. Returns number actually written.
    // Drops incoming data if the buffer is full.
    uint32_t write(const T* data, uint32_t count) {
        const uint32_t w = static_cast<uint32_t>(write_.load(std::memory_order_relaxed));
        const uint32_t space =
            Capacity - static_cast<uint32_t>(w - read_.load(std::memory_order_acquire));
        if (count > space)
            count = space;
        for (uint32_t i = 0; i < count; ++i)
            buf_[(w + i) & kMask] = data[i];
        write_.store(w + count, std::memory_order_release);
        return count;
    }

    // Read up to `count` elements, advancing the read pointer. Returns number read.
    uint32_t read(T* data, uint32_t count) {
        const uint32_t r = static_cast<uint32_t>(read_.load(std::memory_order_relaxed));
        const uint32_t avail =
            static_cast<uint32_t>(write_.load(std::memory_order_acquire) - r);
        if (count > avail)
            count = avail;
        for (uint32_t i = 0; i < count; ++i)
            data[i] = buf_[(r + i) & kMask];
        read_.store(r + count, std::memory_order_release);
        return count;
    }

    // Peek at up to `count` elements without advancing the read pointer.
    uint32_t peek(T* data, uint32_t count) const {
        const uint32_t r = static_cast<uint32_t>(read_.load(std::memory_order_relaxed));
        const uint32_t avail =
            static_cast<uint32_t>(write_.load(std::memory_order_acquire) - r);
        if (count > avail)
            count = avail;
        for (uint32_t i = 0; i < count; ++i)
            data[i] = buf_[(r + i) & kMask];
        return count;
    }

private:
    std::array<T, Capacity> buf_{};
    alignas(64) std::atomic<uint64_t> write_{0};
    alignas(64) std::atomic<uint64_t> read_{0};
};

}  // namespace audio
