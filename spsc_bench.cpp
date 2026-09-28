#include "spsc_ring_buffer.hpp"

#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace {

template <std::size_t Bytes>
struct Payload {
    static_assert(Bytes >= sizeof(std::uint64_t), "Payload is too small");

    std::uint64_t sequence{};
    std::array<std::byte, Bytes - sizeof(std::uint64_t)> data{};
};

template <std::size_t Bytes>
void BM_SpscTransfer(benchmark::State& state) {
    static_assert(sizeof(Payload<Bytes>) == Bytes, "Payload must have the requested size");

    // Function-local static storage is shared by the two benchmark worker threads.
    // Google Benchmark runs benchmark cases sequentially; each successful run drains it.
    static SpscRingBuffer<Payload<Bytes>, 1024> queue;

    std::uint64_t next_sequence = 0;
    std::uint64_t checksum = 0;
    bool sequence_ok = true;

    if (state.thread_index() == 0) {
        for (auto _ : state) {
            Payload<Bytes> payload{};
            payload.sequence = next_sequence++;
            for (std::size_t i = 0; i < payload.data.size(); ++i) {
                payload.data[i] = static_cast<std::byte>((payload.sequence + i) & 0xFFU);
            }

            while (!queue.try_push(payload)) {
            }
        }

        state.SetItemsProcessed(state.iterations());
        state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(Bytes));
    } else {
        std::uint64_t expected_sequence = 0;
        for (auto _ : state) {
            std::optional<Payload<Bytes>> payload;
            while (!(payload = queue.try_pop())) {
            }

            if (payload->sequence != expected_sequence++) {
                sequence_ok = false;
            }
            for (const std::byte value : payload->data) {
                checksum += std::to_integer<std::uint8_t>(value);
            }
        }

        benchmark::DoNotOptimize(checksum);
        if (!sequence_ok) {
            state.SkipWithError("SPSC payload sequence was corrupted or reordered");
        }
    }
}

BENCHMARK_TEMPLATE(BM_SpscTransfer, 64)->Threads(2)->UseRealTime();
BENCHMARK_TEMPLATE(BM_SpscTransfer, 256)->Threads(2)->UseRealTime();
BENCHMARK_TEMPLATE(BM_SpscTransfer, 1024)->Threads(2)->UseRealTime();

}  // namespace

BENCHMARK_MAIN();
