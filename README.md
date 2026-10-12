# SPSC ring buffer

A small header-only, fixed-capacity single-producer/single-consumer FIFO for C++17. The producer uses `try_push()` or `try_emplace()`; the consumer uses `try_pop()`, which returns `std::nullopt` when empty. Push returns `false` when full.

The producer and consumer cursors, and each storage slot, are separated using `std::hardware_destructive_interference_size` to reduce false sharing. This may increase memory use; the queue has `Capacity` usable elements and requires exactly one thread per side. Do not destroy the queue while either side is operating on it.

## Build and test with CMake

The default configuration fetches Google Benchmark v1.9.1 and requires Boost headers for the comparison suite; the first configure requires network access. On macOS, install Boost with `brew install boost`. Configure and build all targets, then run the correctness tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the demo and the five-million-item producer/consumer stress test with:

```sh
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

### Docker development environment (macOS/Linux)

Docker Desktop includes its own CLI. On macOS, if `docker` is not on `PATH`, use Docker Desktop's bundled CLI at `/Applications/Docker.app/Contents/Resources/bin/docker` or add that directory to your `PATH`. Start Docker Desktop, then from the project root:

```sh
docker build -t spsc-dev .
docker run -it --rm --privileged -v "$(pwd):/workspace" spsc-dev bash
```

Inside the container, the project is mounted at `/workspace`; build and test using the CMake commands above. The image installs Ubuntu build tools, Boost and Google Benchmark development packages, and the `perf` tools package. It intentionally does not pin a kernel-specific `linux-tools-6.8.0-146-generic` package: containers share Docker Desktop's Linux VM kernel, and the kernel version is not determined by the Ubuntu image tag.

`--privileged` grants container permissions but does not provide hardware performance counters that Docker Desktop's Linux VM does not expose. `perf record` software CPU-clock sampling may work; verify hardware events with `perf stat -e cycles,instructions,cache-references,cache-misses ...`. Unsupported events require PMU exposure from the VM/host and cannot be fixed by installing packages inside the container.

On this Docker Desktop setup, C++ build and CTest succeeded, but `perf` reported that a tool for the LinuxKit `7.0.14` kernel was unavailable (`linux-tools-7.0.14-linuxkit`). Thus `perf stat` could not run here; `linux-tools-generic` alone did not supply a matching binary. The throughput benchmark executable ran, but Google Benchmark warned that its distro library was built as DEBUG, so those timings are only a container smoke test, not performance results. The container shares the LinuxKit VM kernel; installing an Ubuntu kernel-specific perf package will not change that kernel.

## Google Benchmark suite

The suite compares three equal-capacity (1,024 item) queues with one producer and one consumer: this ring buffer, bounded `std::queue` protected by a mutex, and `boost::lockfree::spsc_queue`. Each is tested with 64 B, 256 B, and 1 KiB payloads.

Throughput cases report explicit `wall_messages_per_second` and `wall_payload_GiB_per_second` counters, measured from the producer's start to the consumer's final dequeue. Separate latency cases report p50/p99/p999 enqueue-to-dequeue latency in nanoseconds from 500,000 messages per case. Latency instrumentation timestamps every message and therefore adds overhead; compare percentiles across implementations, but don't treat them as the uninstrumented queue's latency. The latency message also includes timestamp metadata, so its in-queue footprint is larger than the named payload size.

A five-repetition local run and its caveats are recorded in [benchmark_results.md](benchmark_results.md).

### Profiling on Linux with `perf`

`perf` is a Linux tool and cannot run on this macOS host. On Linux, build with symbols and profile a focused case:

```sh
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-perf --target spsc_bench --parallel
perf stat -e cycles,instructions,cache-references,cache-misses \
	./build-perf/spsc_bench --benchmark_filter='BM_CustomSpscThroughput<64>' --benchmark_min_time=5s
perf record -g --call-graph dwarf -o perf.data -- \
	./build-perf/spsc_bench --benchmark_filter='BM_CustomSpscThroughput<64>' --benchmark_min_time=10s
perf report --stdio
```

On supported processors/kernels, `perf c2c record` followed by `perf c2c report --stdio` can help attribute cache-to-cache/HITM traffic. Compare that report before and after layout changes; ordinary CPU sampling identifies hot code but cannot prove false sharing. Hardware-counter permissions and `perf c2c` support vary by Linux system.

The benchmark executable is built with the default CMake configuration. Run all payload cases with:

```sh
./build/spsc_bench
```

To run a shorter throughput sample or select cases, pass Google Benchmark arguments, for example `./build/spsc_bench --benchmark_min_time=0.5s`. CMake builds the benchmark executable with `-O2 -march=native` on GCC and Clang. Benchmark results vary with CPU load, thread scheduling, and power state; run multiple times on an otherwise idle machine for meaningful comparisons.
