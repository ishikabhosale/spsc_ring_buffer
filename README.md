# SPSC ring buffer

A small header-only, fixed-capacity single-producer/single-consumer FIFO for C++17. The producer uses `try_push()` or `try_emplace()`; the consumer uses `try_pop()`, which returns `std::nullopt` when empty. Push returns `false` when full.

The producer and consumer cursors are independently aligned to 64-byte cache lines. The queue has `Capacity` usable elements and requires exactly one thread per side. Do not destroy the queue while either side is operating on it.

## Build and test with CMake

The default configuration fetches Google Benchmark v1.9.1, so the first configure requires network access. Configure and build all targets, then run the correctness tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the demo and the five-million-item producer/consumer stress test with:

```text
./build/spsc_demo
./build/spsc_thread_test
```

The CTest suite includes focused checks for capacity one, full/empty transitions, repeated wraparound, and move-only payloads, as well as the concurrent stress test. To configure without fetching/building the benchmark dependency:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSPSC_BUILD_BENCHMARKS=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On platforms with ThreadSanitizer support, configure a separate build with `-DCMAKE_CXX_FLAGS="-fsanitize=thread -g" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"` to check the concurrent test for data races.

## Google Benchmark suite

The benchmark runs one producer and one consumer concurrently and transfers fixed-size payloads of 64 B, 256 B, and 1 KiB. Google Benchmark reports `items_per_second` (messages/s), `bytes_per_second`, and `ns/op` using wall-clock timing. Here, `ns/op` is effective wall-clock time per transferred message (the reciprocal of throughput), not an individually timestamped end-to-end message latency; timestamping each message would add measurement overhead.

The benchmark executable is built with the default CMake configuration. Run all payload cases with:

```sh
./build/spsc_bench
```

To run a shorter sample or select payload cases, pass Google Benchmark arguments, for example `./build/spsc_bench --benchmark_min_time=0.5s`. CMake builds the benchmark executable with `-O2 -march=native` on GCC and Clang.
