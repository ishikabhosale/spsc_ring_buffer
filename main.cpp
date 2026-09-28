#include "spsc_ring_buffer.hpp"

#include <iostream>

int main() {
    SpscRingBuffer<int, 4> queue;

    for (int value = 1; value <= 4; ++value) {
        if (!queue.try_push(value)) {
            std::cerr << "Unexpected full queue\n";
            return 1;
        }
    }

    if (queue.try_push(5)) {
        std::cerr << "Expected the queue to be full\n";
        return 1;
    }

    while (auto value = queue.try_pop()) {
        std::cout << *value << '\n';
    }

    return 0;
}
