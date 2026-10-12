#include "spsc_ring_buffer.hpp"

#include <benchmark/benchmark.h>
#include <boost/lockfree/spsc_queue.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kQueueCapacity = 1024;
constexpr std::uint64_t kSequenceMask = 0xFFU;

template <std::size_t Bytes>
struct Payload {
    static_assert(Bytes >= sizeof(std::uint64_t), "Payload is too small");

    std::uint64_t sequence{};
    std::array<std::byte, Bytes - sizeof(std::uint64_t)> data{};
};

template <typename T>
class CustomSpscQueue {
public:
    bool push(const T& value) { return queue_.try_push(value); }

    bool pop(T& value) {
        auto result = queue_.try_pop();
        if (!result) return false;
        value = std::move(*result);
        return true;
    }

private:
    SpscRingBuffer<T, kQueueCapacity> queue_;
};

template <typename T>
class MutexQueue {
public:
    bool push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() == kQueueCapacity) return false;
        queue_.push(value);
        return true;
    }

    bool pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
};

template <typename T>
class BoostSpscQueue {
public:
    bool push(const T& value) { return queue_.push(value); }
    bool pop(T& value) { return queue_.pop(value); }

private:
    boost::lockfree::spsc_queue<T, boost::lockfree::capacity<kQueueCapacity>> queue_;
};

struct ThroughputStats {
    std::atomic<std::int64_t> start_ns{0};
};

using SteadyClock = std::chrono::steady_clock;

std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               SteadyClock::now().time_since_epoch())
        .count();
}

template <typename T>
void fill_payload(T& payload, std::uint64_t sequence) {
    payload.sequence = sequence;
    for (std::size_t i = 0; i < payload.data.size(); ++i) {
        payload.data[i] = static_cast<std::byte>((sequence + i) & kSequenceMask);
    }
}

template <std::size_t Bytes, template <typename> class Queue>
void run_throughput(benchmark::State& state, const char* implementation) {
    static_assert(sizeof(Payload<Bytes>) == Bytes, "Payload must have the requested size");
    static Queue<Payload<Bytes>> queue;
    static ThroughputStats stats;

    if (state.thread_index() == 0) {
        stats.start_ns.store(now_ns(), std::memory_order_release);
        std::uint64_t sequence = 0;
        for (auto _ : state) {
            Payload<Bytes> payload{};
            fill_payload(payload, sequence++);
            while (!queue.push(payload)) {
            }
        }
    } else {
        std::uint64_t expected_sequence = 0;
        std::uint64_t checksum = 0;
        bool sequence_ok = true;
        Payload<Bytes> payload{};
        for (auto _ : state) {
            while (!queue.pop(payload)) {
            }
            if (payload.sequence != expected_sequence++) sequence_ok = false;
            for (const std::byte value : payload.data) {
                checksum += std::to_integer<std::uint8_t>(value);
            }
        }

        benchmark::DoNotOptimize(checksum);
        if (!sequence_ok) {
            state.SkipWithError("Payload sequence was corrupted or reordered");
        }

        const double elapsed_seconds = static_cast<double>(now_ns() -
            stats.start_ns.load(std::memory_order_acquire)) / 1.0e9;
        const double messages = static_cast<double>(state.iterations());
        state.counters["wall_messages_per_second"] = messages / elapsed_seconds;
        state.counters["wall_payload_GiB_per_second"] =
            messages * static_cast<double>(Bytes) / elapsed_seconds / (1024.0 * 1024.0 * 1024.0);
        state.SetLabel(implementation);
    }
}

template <std::size_t Bytes>
struct TimedPayload : Payload<Bytes> {
    std::int64_t enqueue_ns{};
};

template <std::size_t Bytes, template <typename> class Queue>
void run_latency(benchmark::State& state, const char* implementation) {
    using Message = TimedPayload<Bytes>;
    static_assert(std::is_trivially_copyable<Message>::value,
                  "Boost SPSC latency message must be trivially copyable");
    static Queue<Message> queue;

    if (state.thread_index() == 0) {
        std::uint64_t sequence = 0;
        for (auto _ : state) {
            Message payload{};
            fill_payload(payload, sequence++);
            payload.enqueue_ns = now_ns();
            while (!queue.push(payload)) {
            }
        }
    } else {
        std::uint64_t expected_sequence = 0;
        std::uint64_t checksum = 0;
        bool sequence_ok = true;
        std::vector<std::int64_t> latencies;
        latencies.reserve(static_cast<std::size_t>(state.iterations()));
        Message payload{};
        for (auto _ : state) {
            while (!queue.pop(payload)) {
            }
            if (payload.sequence != expected_sequence++) sequence_ok = false;
            latencies.push_back(now_ns() - payload.enqueue_ns);
            for (const std::byte value : payload.data) {
                checksum += std::to_integer<std::uint8_t>(value);
            }
        }

        benchmark::DoNotOptimize(checksum);
        if (!sequence_ok) {
            state.SkipWithError("Payload sequence was corrupted or reordered");
        }

        std::sort(latencies.begin(), latencies.end());
        const auto percentile = [&latencies](double p) {
            const auto index = static_cast<std::size_t>(p * static_cast<double>(latencies.size() - 1));
            return static_cast<double>(latencies[index]);
        };
        state.counters["p50_ns"] = percentile(0.50);
        state.counters["p99_ns"] = percentile(0.99);
        state.counters["p999_ns"] = percentile(0.999);
        state.counters["latency_samples"] = static_cast<double>(latencies.size());
        state.SetLabel(implementation);
    }
}

template <std::size_t Bytes>
void BM_CustomSpscThroughput(benchmark::State& s) { run_throughput<Bytes, CustomSpscQueue>(s, "custom_spsc"); }
template <std::size_t Bytes>
void BM_MutexThroughput(benchmark::State& s) { run_throughput<Bytes, MutexQueue>(s, "std_queue_mutex"); }
template <std::size_t Bytes>
void BM_BoostSpscThroughput(benchmark::State& s) { run_throughput<Bytes, BoostSpscQueue>(s, "boost_lockfree_spsc"); }

template <std::size_t Bytes>
void BM_CustomSpscLatency(benchmark::State& s) { run_latency<Bytes, CustomSpscQueue>(s, "custom_spsc"); }
template <std::size_t Bytes>
void BM_MutexLatency(benchmark::State& s) { run_latency<Bytes, MutexQueue>(s, "std_queue_mutex"); }
template <std::size_t Bytes>
void BM_BoostSpscLatency(benchmark::State& s) { run_latency<Bytes, BoostSpscQueue>(s, "boost_lockfree_spsc"); }

#define REGISTER_THROUGHPUT(Bytes) \
    BENCHMARK_TEMPLATE(BM_CustomSpscThroughput, Bytes)->Threads(2)->UseRealTime(); \
    BENCHMARK_TEMPLATE(BM_MutexThroughput, Bytes)->Threads(2)->UseRealTime(); \
    BENCHMARK_TEMPLATE(BM_BoostSpscThroughput, Bytes)->Threads(2)->UseRealTime();

#define REGISTER_LATENCY(Bytes) \
    BENCHMARK_TEMPLATE(BM_CustomSpscLatency, Bytes)->Threads(2)->UseRealTime()->Iterations(500'000); \
    BENCHMARK_TEMPLATE(BM_MutexLatency, Bytes)->Threads(2)->UseRealTime()->Iterations(500'000); \
    BENCHMARK_TEMPLATE(BM_BoostSpscLatency, Bytes)->Threads(2)->UseRealTime()->Iterations(500'000);

REGISTER_THROUGHPUT(64);
REGISTER_THROUGHPUT(256);
REGISTER_THROUGHPUT(1024);
REGISTER_LATENCY(64);
REGISTER_LATENCY(256);
REGISTER_LATENCY(1024);

#undef REGISTER_THROUGHPUT
#undef REGISTER_LATENCY

}  // namespace

BENCHMARK_MAIN();
