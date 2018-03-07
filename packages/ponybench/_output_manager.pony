use "format"
use "term"

interface _OutputManager
  fun ref apply(results: _Results)

class _TerminalOutput is _OutputManager
  let _env: Env
  let _noadjust: Bool
  var _overhead_mean: F64 = 0
  var _overhead_median: F64 = 0

  new create(env: Env) =>
    _env = env
    _noadjust = _env.args.contains("--noadjust", {(a, b) => a == b })
    if not _noadjust then
      _print("Benchmark results will have their mean and median adjusted for overhead.")
      _print("You may disable this with --noadjust.\n")
    end
    _print_heading()

  fun ref apply(results: _Results) =>
    if results.overhead then
      if _noadjust then
        _print_benchmark(consume results, false)
      else
        _overhead_mean = results.mean() / results.iterations.f64()
        _overhead_median = results.median() / results.iterations.f64()
      end
    else
      _print_benchmark(consume results, not _noadjust)
    end

  fun ref _print_benchmark(results: _Results, adjust: Bool) =>
    let iters = results.iterations.f64()
    let mean' = results.mean()
    var mean = mean' / iters
    var median = results.median() / iters
    if adjust then
      mean = mean - _overhead_mean
      median =  median - _overhead_median
    end

    let std_dev = results.std_dev()
    let relative_std_dev = (std_dev * 100) / mean'

    _print_result(
      results.name,
      mean.round().i64().string(),
      median.round().i64().string(),
      Format.float[F64](relative_std_dev where prec = 2, fmt = FormatFix),
      iters.u64().string())

    if (mean.round() < 0) or (median.round() < 0) then
      _warn("Adjustment for overhead has resulted in negative values.")
    end

  fun _print_heading() =>
    _print("".join(
      [ ANSI.bold()
        Format("Benchmark" where width = 30)
        Format("mean" where width = 18, align = AlignRight)
        Format("median" where width = 18, align = AlignRight)
        Format("deviation" where width = 12, align = AlignRight)
        Format("iterations" where width = 12, align = AlignRight)
        ANSI.reset()
      ].values()))

  fun _print_result(
    name: String,
    mean: String,
    median: String,
    dev: String,
    iters: String)
  =>
    _print("".join(
      [ Format(name where width = 30)
        Format(mean + " ns" where width = 18, align = AlignRight)
        Format(median + " ns" where width = 18, align = AlignRight)
        Format("±" + dev + "%" where width = 13, align = AlignRight)
        Format(iters where width = 12, align = AlignRight)
      ].values()))

  fun _print(msg: String) =>
    _env.out.print(msg)

  fun _warn(msg: String) =>
    _print(ANSI.yellow() + ANSI.bold() + "Warning: " + msg + ANSI.reset())

// TODO document
// overhead, results...
// name, results...
class _CSVOutput
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun ref apply(results: _Results) =>
    _print(results.raw_str())

  fun _print(msg: String) =>
    _env.out.print(msg)
