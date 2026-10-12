# SPSC comparison benchmark results

Captured locally on 2026-10-05 with AppleClang 21.0.0, Release, `-O2 -march=native`; Boost 1.92.0 and Google Benchmark 1.9.1. Each implementation was run with one producer and one consumer, a 1,024-item queue, and 64 B, 256 B, or 1 KiB payloads. The custom queue uses destructive-interference padding. Throughput is the median of five repetitions (`--benchmark_min_time=0.5s`). Latency used 500,000 timestamped messages per repetition; table values are medians across five repetitions.

## Throughput

| Payload | Implementation | Messages/s | Payload GiB/s |
| --- | --- | ---: | ---: |
| 64 B | Custom SPSC | 15.03 M | 0.896 |
| 64 B | `std::queue` + mutex | 4.53 M | 0.270 |
| 64 B | Boost.Lockfree SPSC | 11.75 M | 0.700 |
| 256 B | Custom SPSC | 12.93 M | 3.083 |
| 256 B | `std::queue` + mutex | 1.98 M | 0.473 |
| 256 B | Boost.Lockfree SPSC | 13.92 M | 3.318 |
| 1 KiB | Custom SPSC | 3.82 M | 3.640 |
| 1 KiB | `std::queue` + mutex | 1.36 M | 1.296 |
| 1 KiB | Boost.Lockfree SPSC | 2.88 M | 2.749 |

## Enqueue-to-dequeue latency

All values are nanoseconds. These are medians across five repetitions of the per-run p50/p99/p999 statistics.

| Payload | Implementation | p50 | p99 | p999 |
| --- | --- | ---: | ---: | ---: |
| 64 B | Custom SPSC | 53,375 | 103,417 | 178,167 |
| 64 B | `std::queue` + mutex | 8,291 | 133,833 | 239,459 |
| 64 B | Boost.Lockfree SPSC | 167 | 67,250 | 102,125 |
| 256 B | Custom SPSC | 15,583 | 118,459 | 162,959 |
| 256 B | `std::queue` + mutex | 66,292 | 325,500 | 368,791 |
| 256 B | Boost.Lockfree SPSC | 3,750 | 96,542 | 121,792 |
| 1 KiB | Custom SPSC | 250 | 16,042 | 57,625 |
| 1 KiB | `std::queue` + mutex | 2,443,670 | 4,372,750 | 4,767,000 |
| 1 KiB | Boost.Lockfree SPSC | 458 | 8,875 | 43,625 |

## How to interpret these numbers

- The explicit throughput counters use wall time measured from before the producer starts until the consumer dequeues the last item. Use these counters rather than Google Benchmark's default `Time` column for aggregate messages/s.
- Latency records every message and includes time spent waiting in the queue. Both workers run flat-out; scheduling imbalance can let the producer build a backlog (up to the 1,024-item capacity), so these results are saturated queueing latency, not unloaded per-operation latency.
- The timestamped latency message includes an extra timestamp field and measuring every message adds overhead. Do not directly compare latency values with the uninstrumented throughput measurements.
- The run reported that it could not read the CPU clock rate and could not set thread affinity. CPU frequency metadata is therefore not trustworthy, and thread placement/scheduling adds variability. Repeat on an otherwise idle system; treat the results as a local snapshot, not a general performance claim.
- A 60-second isolated 1 KiB custom-SPSC run measured 2.16 M messages/s before slot/cursor padding and 3.97 M/s afterward. Other five-repetition results are in the tables above; host load differed between runs, so this is suggestive, not a controlled causal measurement.

See [profiling_notes.md](profiling_notes.md) for the macOS sampler findings, the false-sharing layout analysis, and Linux `perf` instructions.

## EC2 results (Amazon Linux 2023)

Captured on 2026-10-12 on an x86_64 EC2 VM with 2 vCPUs, 2.5 GHz reported CPU frequency, GCC 11.5.0, Boost 1.75.0, and Google Benchmark 1.9.1. The queue capacity was 1,024. Throughput figures are medians of three repetitions with `--benchmark_min_time=0.2s`; the latency benchmark used 500,000 timestamped messages per case. Treat this as a short-run VM snapshot; it is not directly comparable to the Apple Silicon results above.

### Throughput

| Payload | Implementation | Messages/s | Payload GiB/s |
| --- | --- | ---: | ---: |
| 64 B | Custom SPSC | 20.65 M | 1.231 |
| 64 B | `std::queue` + mutex | 2.59 M | 0.154 |
| 64 B | Boost.Lockfree SPSC | 13.34 M | 0.795 |
| 256 B | Custom SPSC | 4.93 M | 1.176 |
| 256 B | `std::queue` + mutex | 1.64 M | 0.392 |
| 256 B | Boost.Lockfree SPSC | 5.04 M | 1.201 |
| 1 KiB | Custom SPSC | 1.31 M | 1.245 |
| 1 KiB | `std::queue` + mutex | 1.07 M | 1.017 |
| 1 KiB | Boost.Lockfree SPSC | 1.34 M | 1.278 |

### Enqueue-to-dequeue latency

Nanoseconds; medians of the per-run percentiles across three repetitions.

| Payload | Implementation | p50 | p99 | p999 |
| --- | --- | ---: | ---: | ---: |
| 64 B | Custom SPSC | 84,497 | 273,007 | 1,951,420 |
| 64 B | `std::queue` + mutex | 370,574 | 471,716 | 823,986 |
| 64 B | Boost.Lockfree SPSC | 66,459 | 4,869,350 | 39,897,400 |
| 256 B | Custom SPSC | 249,500 | 2,172,740 | 3,975,960 |
| 256 B | `std::queue` + mutex | 852,021 | 1,125,850 | 2,448,070 |
| 256 B | Boost.Lockfree SPSC | 8,709 | 227,869 | 373,555 |
| 1 KiB | Custom SPSC | 816,149 | 906,229 | 1,232,650 |
| 1 KiB | `std::queue` + mutex | 364,145 | 650,743 | 1,004,470 |
| 1 KiB | Boost.Lockfree SPSC | 129 | 259,793 | 294,357 |

The latency measurements are highly variable and include saturated queueing delay and per-message timestamping overhead; notably, some tail percentiles varied substantially across only three runs. Do not infer a generally faster queue from these latency figures.

### `perf` findings

`perf record -F 99 -g --call-graph dwarf` succeeded and captured 521 CPU-clock samples for the 64 B custom SPSC benchmark. The sampled hotspot was the inlined `run_throughput` producer/consumer loop; optimized inlining did not separate individual queue operations well.

`perf stat` was retried both as `ec2-user` and with `sudo` for `cycles`, `instructions`, `cache-references`, and `cache-misses`. All four events reported `<not supported>` even as root. The result indicates this VM did not expose these hardware PMU events to the guest; changing Linux perf permissions was not enough. Consequently, these runs do not measure hardware cache misses or establish that false sharing caused any observed performance difference. Use a host with PMU access, and ideally `perf c2c`, for that analysis.
