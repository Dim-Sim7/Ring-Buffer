Ring Buffer

A lock-free single-producer single-consumer (SPSC) ring buffer in C++, built
iteratively with a focus on low-latency performance. Each version introduces one
optimisation and benchmarks the result, so the repo doubles as a study of how
hardware and the C++ memory model affect throughput.

Implementations

1. Naive - baseline implementation with no thread safety.

2. Threads - adds atomics with default sequential consistency (memory_order_seq_cst),
making it thread-safe but with unnecessary synchronisation overhead.

3. Array alignment (alignas) - cache-line aligns the read and write indices
to eliminate false sharing between the producer and consumer threads.

4. Memory ordering - replaces seq_cst with acquire/release semantics.
The write index uses release on store and the read index uses acquire on load,
removing the global ordering constraint and reducing cache-coherence traffic.

5. Cached reads - the producer caches a local copy of the read index and the
consumer caches the write index, cutting down on cross-core atomic loads in the
common case where the buffer is not near full or empty.

6. Further optimisations - additional tuning on top of version 5.

Benchmarks

Benchmarks run at 10,000,000 iterations on Linux with perf. Example result from
version 4 (memory ordering):

2.13e+08 ops/sec
491M cycles   304M instructions
9.0M cache references   6.5M cache misses
0.048s wall time

Cache misses drop significantly between versions as false sharing is eliminated
and the cached-index optimisation reduces cross-core coherence traffic.

Building

Change the #include at the top of benchmarkbuffer.cpp or benchmarkburst.cpp
to point to whichever implementation you want to test, then build:

bashcmake -B build/Debug
cmake --build build/Debug
./build/Debug/bench
./build/Debug/benchburst