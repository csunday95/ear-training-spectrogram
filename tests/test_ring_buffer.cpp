#include <catch2/catch_test_macros.hpp>

#include "ring_buffer.hpp"

#include <numeric>
#include <thread>

using audio::RingBuffer;

TEST_CASE("RingBuffer basic read/write", "[ring_buffer]") {
    RingBuffer<float, 16> rb;
    REQUIRE(rb.available() == 0);

    const float in[4] = {1.f, 2.f, 3.f, 4.f};
    REQUIRE(rb.write(in, 4) == 4);
    REQUIRE(rb.available() == 4);

    float out[4] = {};
    REQUIRE(rb.read(out, 4) == 4);
    REQUIRE(out[0] == 1.f);
    REQUIRE(out[3] == 4.f);
    REQUIRE(rb.available() == 0);
}

TEST_CASE("RingBuffer partial read", "[ring_buffer]") {
    RingBuffer<float, 16> rb;
    float in[8];
    std::iota(in, in + 8, 0.f);
    rb.write(in, 8);

    float out[4] = {};
    REQUIRE(rb.read(out, 4) == 4);
    REQUIRE(rb.available() == 4);
    REQUIRE(out[0] == 0.f);
    REQUIRE(out[3] == 3.f);
}

TEST_CASE("RingBuffer wrap-around", "[ring_buffer]") {
    RingBuffer<float, 8> rb;

    float in[6];
    std::iota(in, in + 6, 0.f);
    rb.write(in, 6);

    float dummy[4] = {};
    rb.read(dummy, 4);  // consume 4; write index=6, read index=4

    // Write 6 more — wraps around the end of the backing array
    float in2[6];
    std::iota(in2, in2 + 6, 10.f);
    REQUIRE(rb.write(in2, 6) == 6);  // 2 remaining + 6 new = 8 (full)

    float out[8] = {};
    REQUIRE(rb.read(out, 8) == 8);
    // First 2 should be in[4] and in[5]
    REQUIRE(out[0] == 4.f);
    REQUIRE(out[1] == 5.f);
    // Then in2[0..5]
    REQUIRE(out[2] == 10.f);
    REQUIRE(out[7] == 15.f);
}

TEST_CASE("RingBuffer overflow drops new data", "[ring_buffer]") {
    RingBuffer<float, 4> rb;
    const float in[4] = {1.f, 2.f, 3.f, 4.f};
    REQUIRE(rb.write(in, 4) == 4);
    REQUIRE(rb.available() == 4);

    // Buffer is full — write returns 0 and original data is preserved
    const float extra[2] = {5.f, 6.f};
    REQUIRE(rb.write(extra, 2) == 0);
    REQUIRE(rb.available() == 4);

    float out[4] = {};
    rb.read(out, 4);
    REQUIRE(out[0] == 1.f);
    REQUIRE(out[3] == 4.f);
}

TEST_CASE("RingBuffer peek does not advance read pointer", "[ring_buffer]") {
    RingBuffer<float, 8> rb;
    const float in[4] = {1.f, 2.f, 3.f, 4.f};
    rb.write(in, 4);

    float out[4] = {};
    REQUIRE(rb.peek(out, 4) == 4);
    REQUIRE(rb.available() == 4);  // not consumed
    REQUIRE(out[0] == 1.f);

    float out2[4] = {};
    rb.peek(out2, 4);
    REQUIRE(out2[0] == 1.f);  // same data on second peek
}

TEST_CASE("RingBuffer concurrent SPSC", "[ring_buffer]") {
    constexpr uint32_t kTotal = 65536;
    RingBuffer<float, 4096> rb;

    std::thread producer([&] {
        float val = 0.f;
        uint32_t written = 0;
        while (written < kTotal) {
            float buf[64];
            const uint32_t batch = std::min(64u, kTotal - written);
            for (uint32_t i = 0; i < batch; ++i)
                buf[i] = val + static_cast<float>(i);
            const uint32_t w = rb.write(buf, batch);
            val += static_cast<float>(w);
            written += w;
        }
    });

    float prev = -1.f;
    uint32_t total_read = 0;
    bool monotonic = true;
    while (total_read < kTotal) {
        float buf[64];
        const uint32_t got = rb.read(buf, 64);
        for (uint32_t i = 0; i < got; ++i) {
            if (buf[i] <= prev)
                monotonic = false;
            prev = buf[i];
        }
        total_read += got;
    }

    producer.join();
    REQUIRE(monotonic);
    REQUIRE(total_read == kTotal);
}
