
type Benchmark is (MicroBenchmark | AsyncMicroBenchmark)

// interface iso _IBenchmark
//   fun box name(): String
//   fun box config(): BenchConfig
//   fun box overhead(): _IBenchmark^

trait iso MicroBenchmark
  """
  Synchronous benchmarks must provide this trait. The `apply` method defines a
  single iteration in a sample. Setup and Teardown are defined by the `before`
  and `after` methods respectively. The `before` method runs before a sample
  of benchmarks and `after` runs after the all iterations in the sample have
  completed. If your benchmark requires setup and/or teardown to occur beween
  each iteration of the benchmark, then you must set the configuration of the
  benchmark to have `max_iterations = 1`. It should be noted that a larger
  `sample_size` may be necessary in this scenario for statistically
  significant results.
  """
  fun box name(): String
  fun box config(): BenchConfig => BenchConfig
  fun box overhead(): MicroBenchmark^ => OverheadBenchmark
  fun ref before() => None
  fun ref apply() ?
  fun ref after() => None

trait iso AsyncMicroBenchmark
  """
  Asynchronous benchmarks must provide this trait. The `apply` method defines a
  single iteration in a sample. Each phase of the sample completes when the
  given `AsyncBenchContinue` has its `complete` method invoked. Setup and
  Teardown are defined by the `before` and `after` methods respectively. The
  `before` method runs before a sample of benchmarks and `after` runs after
  the all iterations in the sample have completed. If your benchmark requires
  setup and/or teardown to occur beween each iteration of the benchmark, then
  you must set the configuration of the benchmark to have `max_iterations = 1`.
  It should be noted that a larger `sample_size` may be necessary in this
  scenario for statistically significant results.
  """
  fun box name(): String
  fun box config(): BenchConfig => BenchConfig
  fun box overhead(): AsyncMicroBenchmark^ => AsyncOverheadBenchmark
  fun ref before(c: AsyncBenchContinue) => c.complete()
  fun ref apply(c: AsyncBenchContinue) ?
  fun ref after(c: AsyncBenchContinue) => c.complete()

interface tag BenchmarkList
  fun tag benchmarks(bench: PonyBench)

// TODO documentation
class val BenchConfig
  """
  Configuration of a benchmark.
  - `samples`: total samples measured
  - `max_iterations`: maximum iterations per sample
  - `max_sample_time`: maximum time per sample
  """
  let samples: USize
  let max_iterations: U64
  let max_sample_time: U64

  new val create(
    samples': USize = 20,
    max_iterations': U64 = 1_000_000_000,
    max_sample_time': U64 = 100_000_000)
  =>
    samples = samples'
    max_iterations = max_iterations'
    max_sample_time = max_sample_time'

class iso OverheadBenchmark is MicroBenchmark
  """
  Default benchmark for measuring synchronous overhead.
  """
  fun name(): String =>
    "Benchmark Overhead"

  fun ref apply() =>
    DoNotOptimise[None](None)
    DoNotOptimise.observe()

class iso AsyncOverheadBenchmark is AsyncMicroBenchmark
  """
  Default benchmark for measuring asynchronous overhead.
  """
  fun name(): String =>
    "Benchmark Overhead"

  fun ref apply(c: AsyncBenchContinue) =>
    c.complete()
