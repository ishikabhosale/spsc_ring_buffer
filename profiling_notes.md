# Profiling and false-sharing investigation

## Platform and profiler

The local host is Apple Silicon running macOS (`Darwin arm64`). Linux `perf` is not installed or supported here. `xctrace` requires the full Xcode developer directory; this host is using Command Line Tools only. I used macOS `/usr/bin/sample` to collect 10-second CPU call-stack samples of the optimized 1 KiB custom SPSC throughput case. These are CPU samples, not hardware-counter or cache-to-cache measurements.

## Sampled hotspots

The samples spend most of their time inside the benchmark's inlined producer/consumer hot loops; optimized code labels much of that work as an outlined function. `memmove`/`memcpy` also appears in both producer and consumer stacks, consistent with copying the 1 KiB payload through the queue. A normal stack sampler cannot distinguish cache misses, coherence stalls, or false sharing. On Linux, use `perf stat` for counters, `perf record/report` for stacks, and (if supported) `perf c2c` to inspect cache-to-cache/HITM traffic.

## False-sharing finding and fix

The kernel reports `hw.cachelinesize = 128` on this machine. The queue previously aligned each cursor to 64 bytes, so the producer-owned and consumer-owned cursor objects could occupy the same 128-byte cache line. Adjacent payload slots were also packed together. The header now uses `std::hardware_destructive_interference_size` for both cursor separation and per-slot alignment. This value is 256 bytes in the installed Apple standard library, conservatively separating data for the 128-byte hardware line.

This is a layout-based false-sharing fix, not a claim that the sample profiler directly proved cache-to-cache contention. Validate the contention hypothesis on a Linux machine with `perf c2c` before/after. The padding also increases queue memory consumption.

A 60-second isolated 1 KiB custom-SPSC run measured 2.16 M messages/s before padding and 3.97 M/s afterward. These runs differed in host load and thread scheduling, so the improvement is indicative, not a controlled causal measurement.
