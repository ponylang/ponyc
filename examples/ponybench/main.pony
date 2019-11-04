use "ponybench"
use "time"

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
