"""
PonyBench provides a microbenchmarking framework. It is designed to measure
the runtime of synchronous and asynchronous operations.

## Example Program

The following is a complete program with multiple trivial benchmarks followed
by their output.

```pony
use "time"
use "pony_bench"

actor Main is BenchmarkList
  new create(env: Env) =>
    PonyBench(env, this)

  fun tag benchmarks(bench: PonyBench) =>
    bench(_Nothing)
    bench(_Fib(5))
    bench(_Fib(10))
    bench(_Fib(20))
    bench(_Timer(10_000))

class iso _Nothing is MicroBenchmark
  // Benchmark absolutely nothing.
  fun name(): String => "Nothing"

  fun apply() =>
    // prevent compiler from optimizing out this operation
    DoNotOptimise[None](None)
    DoNotOptimise.observe()

class iso _Fib is MicroBenchmark
  // Benchmark non-tail-recursive fibonacci
  let _n: U64

  new iso create(n: U64) =>
    _n = n

  fun name(): String =>
    "_Fib(" + _n.string() + ")"

  fun apply() =>
    DoNotOptimise[U64](_fib(_n))
    DoNotOptimise.observe()

  fun _fib(n: U64): U64 =>
    if n < 2 then 1
    else _fib(n - 1) + _fib(n - 2)
    end

class iso _Timer is AsyncMicroBenchmark
  // Asynchronous benchmark of timer.
  let _ts: Timers = Timers
  let _ns: U64

  new iso create(ns: U64) =>
    _ns = ns

  fun name(): String =>
    "_Timer (" + _ns.string() + " ns)"

  fun apply(c: AsyncBenchContinue) =>
    _ts(Timer(
      object iso is TimerNotify
        fun apply(timer: Timer, count: U64 = 0): Bool =>
          // signal completion of async benchmark iteration when timer fires
          c.complete()
          false
      end,
      _ns))

```
By default, the results are printed to stdout like so:
```
Benchmark results will have their mean and median adjusted for overhead.
You may disable this with --noadjust.

Benchmark                                   mean            median   deviation  iterations
Nothing                                     1 ns              1 ns      ±0.87%     3000000
_Fib(5)                                    12 ns             12 ns      ±1.02%     2000000
_Fib(10)                                  185 ns            184 ns      ±1.03%     1000000
_Fib(20)                                23943 ns          23898 ns      ±1.11%       10000
_Timer (10000ns)                        10360 ns          10238 ns      ±3.25%       10000
```
The `--noadjust` option outputs results of the overhead measured prior to each
benchmark run followed by the unadjusted benchmark result. An example of the
output of this program with `--noadjust` is as follows:
```
Benchmark                                   mean            median   deviation  iterations
Benchmark Overhead                        604 ns            603 ns      ±0.58%      300000
Nothing                                   553 ns            553 ns      ±0.30%      300000
Benchmark Overhead                        555 ns            555 ns      ±0.51%      300000
_Fib(5)                                   574 ns            574 ns      ±0.43%      300000
Benchmark Overhead                        554 ns            556 ns      ±0.48%      300000
_Fib(10)                                  822 ns            821 ns      ±0.39%      200000
Benchmark Overhead                        554 ns            553 ns      ±0.65%      300000
_Fib(20)                                30470 ns          30304 ns      ±1.55%        5000
Benchmark Overhead                        552 ns            552 ns      ±0.39%      300000
_Timer (10000 ns)                       10780 ns          10800 ns      ±3.60%       10000
```

It is recommended that a PonyBench program is compiled with the `--runtimebc`
option, if possible, and run with the `--ponynoyield` option.
"""
// TODO more examples in tutorial

actor PonyBench
  let _env: Env
  let _output_manager: _OutputManager
  embed _bench_q: Array[(Benchmark, Bool)] = Array[(Benchmark, Bool)]
  var _running: Bool = false

  new create(env: Env, list: BenchmarkList) =>
    _env = consume env
    ifdef debug then
      _env.err.print("***WARNING*** Benchmark was built as DEBUG. Timings may be affected.")
    end

    _output_manager =
      if _env.args.contains("-csv", {(a, b) => a == b })
      then _CSVOutput(_env)
      else _TerminalOutput(_env)
      end

    list.benchmarks(this)

  be apply(bench: Benchmark) =>
    match \exhaustive\ consume bench
    | let b: MicroBenchmark =>
      _bench_q.push((b.overhead(), true))
      _bench_q.push((consume b, false))
    | let b: AsyncMicroBenchmark =>
      _bench_q.push((b.overhead(), true))
      _bench_q.push((consume b, false))
    end

    if not _running then
      _running = true
      _next_benchmark()
    end

  be _next_benchmark() =>
    if _bench_q.size() > 0 then
      try
        match _bench_q.shift()?
        | (let b: MicroBenchmark, let overhead: Bool) =>
          _RunSync(this, consume b, overhead)
        | (let b: AsyncMicroBenchmark, let overhead: Bool) =>
          _RunAsync(this, consume b, overhead)
        end
      end
    else
      _running = false
    end

  be _complete(results: _Results) =>
    _output_manager(consume results)
    _next_benchmark()

  be _fail(name: String) =>
    _env.err.print("Failed benchmark: " + name)
    _env.exitcode(1)
