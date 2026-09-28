#include "spsc_ring_buffer.hpp"

#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>

namespace {

bool test_capacity_one() {
    SpscRingBuffer<int, 1> queue;

    if (queue.try_pop().has_value() || !queue.try_push(0) || queue.try_push(1)) {
        return false;
    }
    const auto first = queue.try_pop();
    if (!first || *first != 0 || queue.try_pop().has_value()) {
        return false;
    }

    for (int value = 0; value < 10'000; ++value) {
        if (!queue.try_push(value)) {
            return false;
        }
        const auto popped = queue.try_pop();
        if (!popped || *popped != value) {
            return false;
        }
    }
    return true;
}

bool test_full_empty_and_wraparound() {
    constexpr int kCapacity = 4;
    SpscRingBuffer<int, kCapacity> queue;

    if (queue.try_pop().has_value()) {
        return false;
    }
    for (int value = 0; value < kCapacity; ++value) {
        if (!queue.try_push(value)) {
            return false;
        }
    }
    if (queue.try_push(kCapacity)) {
        return false;
    }
    for (int value = 0; value < kCapacity; ++value) {
        const auto popped = queue.try_pop();
        if (!popped || *popped != value) {
            return false;
        }
    }
    if (queue.try_pop().has_value()) {
        return false;
    }

    for (int cycle = 0; cycle < 10'000; ++cycle) {
        const int base = cycle * kCapacity;
        for (int offset = 0; offset < kCapacity; ++offset) {
            if (!queue.try_push(base + offset)) {
                return false;
            }
        }
        for (int offset = 0; offset < kCapacity; ++offset) {
            const auto popped = queue.try_pop();
            if (!popped || *popped != base + offset) {
                return false;
            }
        }
    }
    return true;
}

bool test_move_only_payload() {
    SpscRingBuffer<std::unique_ptr<int>, 2> queue;
    auto value = std::make_unique<int>(42);

    if (!queue.try_push(std::move(value)) || value ||
        !queue.try_push(std::make_unique<int>(7)) ||
        queue.try_push(std::make_unique<int>(9))) {
        return false;
    }

    auto first = queue.try_pop();
    auto second = queue.try_pop();
    return first && *first && **first == 42 && second && *second && **second == 7 &&
           !queue.try_pop().has_value();
}

}  // namespace

int main() {
    if (!test_capacity_one()) {
        std::cerr << "FAILED: capacity-one full/empty and wraparound\n";
        return 1;
    }
    if (!test_full_empty_and_wraparound()) {
        std::cerr << "FAILED: capacity-four full/empty and repeated wraparound\n";
        return 1;
    }
    if (!test_move_only_payload()) {
        std::cerr << "FAILED: move-only payload\n";
        return 1;
    }

    std::cout << "Passed: capacity-one, full/empty, wraparound, and move-only tests.\n";
    return 0;
}
