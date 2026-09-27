# message-ubench

Measures actor message-passing throughput.

A set of Pinger actors forwards ping messages to randomly chosen peers. A coordinator collects per-interval counts and prints one CSV line per second. At the end it prints the aggregate rate.

The number of in-flight messages is bounded by `initial-pings × pingers`, keeping memory consumption predictable regardless of duration.

## Building

```bash
cd build/release
./ponyc -b message-ubench ../../benchmarks/performance/message-ubench
```

## Running

```bash
./message-ubench --duration 300 --ponymaxthreads 9
```

## Options

| Flag | Default | Description |
|------|---------|-------------|
| `--pingers` | 8 | Number of Pinger actors |
| `--initial-pings` | 5 | Initial pings sent to each Pinger per interval |
| `--duration` | 60 | Benchmark duration in seconds |

Set `--ponymaxthreads` to the number of pingers plus one (for the coordinator).

## Output

```text
interval_ns,messages,messages_per_sec
1000586931,43223797,43198442
999410772,41687279,41711856
# total: 2999806799ns, 101309271 messages, 33771931 msg/s
```
