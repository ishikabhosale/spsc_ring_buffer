#include "spsc_ring_buffer.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>

int main() {
    constexpr std::uint64_t kItems = 5'000'000;
    constexpr auto kTimeout = std::chrono::seconds(30);
    using Clock = std::chrono::steady_clock;

    SpscRingBuffer<std::uint64_t, 1024> queue;
    std::atomic<bool> stop{false};
    const auto deadline = Clock::now() + kTimeout;
    std::uint64_t produced = 0;
    std::uint64_t consumed = 0;
    bool sequence_ok = true;

    std::thread producer([&] {
        std::size_t spins = 0;
        while (produced < kItems && !stop.load(std::memory_order_relaxed)) {
            if (queue.try_push(produced)) {
                ++produced;
            } else if ((++spins & 0xFFFU) == 0 && Clock::now() >= deadline) {
                stop.store(true, std::memory_order_relaxed);
            }
        }
    });

    std::thread consumer([&] {
        std::size_t spins = 0;
        while (consumed < kItems && !stop.load(std::memory_order_relaxed)) {
            if (auto value = queue.try_pop()) {
                if (*value != consumed) {
                    sequence_ok = false;
                }
                ++consumed;
            } else if ((++spins & 0xFFFU) == 0 && Clock::now() >= deadline) {
                stop.store(true, std::memory_order_relaxed);
            }
        }
    });

    producer.join();
    consumer.join();

    if (stop.load(std::memory_order_relaxed) || produced != kItems ||
        consumed != kItems || !sequence_ok) {
        std::cerr << "Thread-safety test failed: produced " << produced << "/" << kItems
                  << ", consumed " << consumed << "/" << kItems
                  << ", sequence " << (sequence_ok ? "OK" : "BROKEN") << '\n';
        return 1;
    }

    std::cout << "Passed: transferred " << kItems
              << " items in order across producer and consumer threads.\n";
    return 0;
}
