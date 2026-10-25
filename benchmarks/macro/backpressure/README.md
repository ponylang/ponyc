# Backpressure Benchmarks

Programs for measuring how the Pony runtime handles backpressure under different topologies. Each subdirectory is a standalone Pony program.

| Benchmark | What it measures |
|-----------|-----------------|
| [overload](overload/) | Sustained overload: 10K senders flooding one receiver |
| [fan-in](fan-in/) | Cascading backpressure: senders -> analyzers -> slow receiver |

## Building

From the build directory:

```bash
cd build/release
./ponyc -b overload ../../benchmarks/macro/backpressure/overload
./ponyc -b fan-in ../../benchmarks/macro/backpressure/fan-in
```

## Running

### overload

```bash
./overload --ponymaxthreads 16
```

Runs until killed. Prints throughput every 50M messages.

### fan-in

```bash
./fan-in --senders 100 --analyzers 2000 --analyzer-interval 100 \
  --analyzer-report-count 10 --receiver-workload 10000 \
  --ponymaxthreads 16
```

| Flag | Default | Description |
|------|---------|-------------|
| `--senders` | 100 | Number of sender actors |
| `--analyzers` | 1000 | Number of analyzer actors |
| `--analyzer-interval` | 100 | Tick interval in centiseconds (100 = 10s) |
| `--analyzer-report-count` | 10 | Number of ticks before shutdown (0 = infinite) |
| `--receiver-workload` | 10000 | Microseconds the receiver sleeps per message |

The default configuration (2000 analyzers, 10s tick, 10ms/msg receiver workload) produces a drain time of 20s per tick, guaranteeing sustained overload on the receiver.
