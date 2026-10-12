#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <new>
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
#if defined(SPSC_CACHE_SEPARATION_BYTES)
    static constexpr std::size_t kCacheSeparation = SPSC_CACHE_SEPARATION_BYTES;
#elif defined(__cpp_lib_hardware_interference_size)
    static constexpr std::size_t kCacheSeparation =
        std::hardware_destructive_interference_size;
#else
    // C++17 libraries without the interference-size constants commonly target
    // 64-byte cache lines. Override by defining SPSC_CACHE_SEPARATION_BYTES.
    static constexpr std::size_t kCacheSeparation = 64;
#endif

    struct alignas(kCacheSeparation) Cursor {
        std::atomic<std::size_t> value{0};
    };

    // Keep independently-owned adjacent elements from sharing a cache line.
    struct alignas(kCacheSeparation) Slot {
        std::optional<T> value;
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

        slots_[write].value.emplace(std::forward<Args>(args)...);
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

        std::optional<T> result(std::move(slots_[read].value.value()));
        slots_[read].value.reset();
        read_cursor_.value.store(next(read), std::memory_order_release);
        return result;
    }

private:
    static constexpr std::size_t next(std::size_t cursor) noexcept {
        return cursor == Capacity ? 0 : cursor + 1;
    }

    // Producer-owned and consumer-owned cursors occupy separate destructive
    // interference regions (128-byte cache lines on the current Apple Silicon host).
    Cursor write_cursor_{};
    Cursor read_cursor_{};
    std::array<Slot, kSlots> slots_{};
};
