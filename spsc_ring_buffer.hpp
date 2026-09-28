#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

// Fixed-capacity, single-producer/single-consumer FIFO.
// The producer may call try_emplace()/try_push(); the consumer may call try_pop().
// Do not call an operation from multiple threads on the same side.
template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity > 0, "Capacity must be greater than zero");
    static_assert(Capacity < std::numeric_limits<std::size_t>::max(),
                  "Capacity is too large");

    // One extra slot distinguishes full from empty while allowing Capacity items.
    static constexpr std::size_t kSlots = Capacity + 1;

    struct alignas(64) Cursor {
        std::atomic<std::size_t> value{0};
    };

public:
    static constexpr std::size_t capacity() noexcept { return Capacity; }

    template <typename... Args>
    bool try_emplace(Args&&... args) {
        const std::size_t write = write_cursor_.value.load(std::memory_order_relaxed);
        const std::size_t next_write = next(write);
        const std::size_t read = read_cursor_.value.load(std::memory_order_acquire);

        if (next_write == read) {
            return false;
        }

        slots_[write].emplace(std::forward<Args>(args)...);
        write_cursor_.value.store(next_write, std::memory_order_release);
        return true;
    }

    template <typename U>
    bool try_push(U&& value) {
        return try_emplace(std::forward<U>(value));
    }

    // Returns an empty optional when the queue is empty.
    std::optional<T> try_pop() {
        const std::size_t read = read_cursor_.value.load(std::memory_order_relaxed);
        const std::size_t write = write_cursor_.value.load(std::memory_order_acquire);

        if (read == write) {
            return std::nullopt;
        }

        std::optional<T> result(std::move(slots_[read].value()));
        slots_[read].reset();
        read_cursor_.value.store(next(read), std::memory_order_release);
        return result;
    }

private:
    static constexpr std::size_t next(std::size_t cursor) noexcept {
        return cursor == Capacity ? 0 : cursor + 1;
    }

    // Producer-owned and consumer-owned cursors occupy separate cache lines.
    Cursor write_cursor_{};
    Cursor read_cursor_{};
    std::array<std::optional<T>, kSlots> slots_{};
};
