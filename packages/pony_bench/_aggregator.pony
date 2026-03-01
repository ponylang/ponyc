
class ref _Aggregator
  let _ponybench: PonyBench
  let _runner: _Runner
  let _overhead: Bool
  let _config: BenchConfig
  var _samples: Array[U64] iso
  var _warmup: Bool = true
  var iterations: U64 = 1

  new create(
    ponybench: PonyBench,
    runner: _Runner,
    config: BenchConfig,
    overhead: Bool)
  =>
    _ponybench = ponybench
    _runner = runner
    _overhead = overhead
    _config = config
    _samples = recover Array[U64](_config.samples) end

  fun ref complete(name: String, t: U64) =>
    if _warmup then
      match \exhaustive\ _calc_iterations(t)
      | let n: U64 => iterations = n
      | None => _warmup = false
      end
      _runner()
    else
      _samples.push(t)
      if _samples.size() < _config.samples then
        _runner()
      else
        _ponybench._complete(_Results(
          name,
          _samples = recover [] end,
          iterations,
          _overhead))
      end
    end

  fun ref _calc_iterations(runtime: U64): (U64 | None) =>
    let max_i = _config.max_iterations
    let max_t = _config.max_sample_time
    let nspi = runtime / iterations
    if (runtime < max_t) and (iterations < max_i) then
      var itrs' =
        if nspi == 0 then max_i
        else max_t / nspi
        end
      itrs' = (itrs' + (itrs' / 5)).min(iterations * 100).max(iterations + 1)
      _round_up(itrs')
    else
      iterations = iterations.min(max_i)
      None
    end

  fun _round_up(x: U64): U64 =>
    """
    Round x up to a number of the form [1^x, 2^x, 3^x, 5^x].
    """
    let base = _round_down_10(x)
    if x <= base then
      base
    elseif x <= (base * 2) then
      base * 2
    elseif x <= (base * 3) then
      base * 3
    elseif x <= (base * 5) then
      base * 5
    else
      base * 10
    end

  fun _round_down_10(x: U64): U64 =>
    """
    Round down to the nearest power of 10.
    """
    let tens = x.f64().log10().floor()
    F64(10).pow(tens).u64()
