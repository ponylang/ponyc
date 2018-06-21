use "time"

trait tag _Runner
  be apply()

actor _RunSync is _Runner
  let _ponybench: PonyBench
  embed _aggregator: _Aggregator
  let _name: String
  let _bench: MicroBenchmark

  new create(
    ponybench: PonyBench,
    benchmark: MicroBenchmark,
    overhead: Bool)
  =>
    _ponybench = ponybench
    _aggregator = _Aggregator(_ponybench, this, benchmark.config(), overhead)
    _name = benchmark.name()
    _bench = consume benchmark
    apply()

  be apply() =>
    _bench.before()
    _gc_next_behavior()
    _run_iteration()

  be _run_iteration(n: U64 = 0, a: U64 = 0) =>
    if n == _aggregator.iterations then
      _complete(a)
    else
      try
        Time.perf_begin()
        let s = Time.nanos()
        _bench()?
        let e = Time.nanos()
        Time.perf_end()
        _run_iteration(n + 1, a + (e - s))
      else
        _fail()
      end
    end

  be _complete(t: U64) =>
    _bench.after()
    _aggregator.complete(_name, t)

  be _fail() =>
    _ponybench._fail(_name)

  fun ref _gc_next_behavior() =>
    @pony_triggergc[None](@pony_ctx[Pointer[None]]())

actor _RunAsync is _Runner
  let _ponybench: PonyBench
  embed _aggregator: _Aggregator
  let _name: String
  let _bench: AsyncMicroBenchmark ref
  var _start_time: U64 = 0
  var _n: U64 = 0
  var _a: U64 = 0

  embed _before_cont: AsyncBenchContinue =
    AsyncBenchContinue._create(this, recover this~_apply_cont() end)
  embed _bench_cont: AsyncBenchContinue =
    AsyncBenchContinue._create(this, recover this~_run_iteration() end)
  embed _after_cont: AsyncBenchContinue =
    AsyncBenchContinue._create(this, recover this~_complete_cont() end)

  new create(
    ponybench: PonyBench,
    benchmark: AsyncMicroBenchmark,
    overhead: Bool)
  =>
    _ponybench = ponybench
    _aggregator = _Aggregator(_ponybench, this, benchmark.config(), overhead)
    _name = benchmark.name()
    _bench = consume benchmark
    apply()

  be apply() =>
    _bench.before(_before_cont)

  be _apply_cont(e: U64) =>
    _n = 0
    _a = 0
    _start_time = 0
    _gc_next_behavior()
    _run_iteration(0)

  be _run_iteration(e: U64) =>
    if _start_time > 0 then
      _a = _a + (e - _start_time)
    end
    if _n == _aggregator.iterations then
      _complete()
    else
      try
        _n = _n + 1
        Time.perf_begin()
        _start_time = Time.nanos()
        _bench(_bench_cont)?
      else
        _fail()
      end
    end

  be _complete() =>
    _bench.after(_after_cont)

  be _complete_cont(e: U64) =>
    _aggregator.complete(_name, _a)

  be _fail() =>
    _ponybench._fail(_name)

  fun ref _gc_next_behavior() =>
    @pony_triggergc[None](@pony_ctx[Pointer[None]]())

class val AsyncBenchContinue
  let _run_async: _RunAsync
  let _f: {(U64)} val

  new val _create(run_async: _RunAsync, f: {(U64)} val) =>
    _run_async = run_async
    _f = f

  fun complete() =>
    let e = Time.nanos()
    Time.perf_end()
    _f(e)

  fun fail() =>
    _run_async._fail()
