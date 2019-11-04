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
    try
      _bench.before()?
      _gc_next_behavior()
      _run_iteration()
    else
      _fail()
    end

  be _run_iteration(n: U64 = 0, a: U64 = 0) =>
    if n == _aggregator.iterations then
      _complete(a)
    else
      try
        _bench.before_iteration()?
        let s =
          ifdef x86 then
            Time.perf_begin()
          else
            Time.nanos()
          end
        _bench()?
        let e =
          ifdef x86 then
            Time.perf_end()
          else
            Time.nanos()
          end
         _bench.after_iteration()?
        _run_iteration(n + 1, a + (e - s))
      else
        _fail()
      end
    end

  be _complete(t: U64) =>
    try
      _bench.after()?
      _aggregator.complete(_name, t)
    else
      _fail()
    end

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
    AsyncBenchContinue._create(this, recover this~_before_done_cont() end)
  embed _before_iteration_cont: AsyncBenchContinue =
    AsyncBenchContinue._create(this, recover this~_before_iteration_done_cont() end)
  embed _iteration_cont: AsyncBenchContinue =
    AsyncBenchContinue._create(this, recover this~_iteration_done_cont() end)
  embed _after_iteration_cont: AsyncBenchContinue =
    AsyncBenchContinue._create(this, recover this~_after_iteration_done_cont() end)
  embed _after_cont: AsyncBenchContinue =
    AsyncBenchContinue._create(this, recover this~_after_done_cont() end)

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

  be _before_done_cont(e: U64) =>
    _n = 0
    _a = 0
    _start_time = 0
    _bench.before_iteration(_before_iteration_cont)

  be _before_iteration_done_cont(e: U64) =>
    _run_iteration()

  be _run_iteration() =>
    try
      _n = _n + 1
      _gc_next_behavior()
      _start_time = Time.nanos()
      _bench(_iteration_cont)?
    else
      _fail()
    end

  be _iteration_done_cont(e: U64) =>
    _a = _a + (e - _start_time)
    _bench.after_iteration(_after_iteration_cont)

  be _after_iteration_done_cont(e: U64) =>
    if _n == _aggregator.iterations then
      _bench.after(_after_cont)
    else
      _bench.before_iteration(_before_iteration_cont)
    end

  be _after_done_cont(e: U64) =>
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
    _f(e)

  fun fail() =>
    _run_async._fail()
