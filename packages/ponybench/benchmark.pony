
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
  each iteration of the benchmark, then you can use `before_iteration` and
  `after_iteration` methods respectively that run before/after each iteration.
  """
  fun box name(): String
  fun box config(): BenchConfig => BenchConfig
  fun box overhead(): MicroBenchmark^ => OverheadBenchmark
  fun ref before() ? => None
  fun ref before_iteration() ? => None
  fun ref apply() ?
  fun ref after() ? => None
  fun ref after_iteration() ? => None

trait iso AsyncMicroBenchmark
  """
  Asynchronous benchmarks must provide this trait. The `apply` method defines a
  single iteration in a sample. Each phase of the sample completes when the
  given `AsyncBenchContinue` has its `complete` method invoked. Setup and
  Teardown are defined by the `before` and `after` methods respectively. The
  `before` method runs before a sample of benchmarks and `after` runs after
  the all iterations in the sample have completed. If your benchmark requires
  setup and/or teardown to occur beween each iteration of the benchmark, then
  you can use `before_iteration` and `after_iteration` methods respectively
  that run before/after each iteration.
  """
  fun box name(): String
  fun box config(): BenchConfig => BenchConfig
  fun box overhead(): AsyncMicroBenchmark^ => AsyncOverheadBenchmark
  fun ref before(c: AsyncBenchContinue) => c.complete()
  fun ref before_iteration(c: AsyncBenchContinue) => c.complete()
  fun ref apply(c: AsyncBenchContinue) ?
  fun ref after(c: AsyncBenchContinue) => c.complete()
  fun ref after_iteration(c: AsyncBenchContinue) => c.complete()

interface tag BenchmarkList
  fun tag benchmarks(bench: PonyBench)

// TODO documentation
class val BenchConfig
  """
  Configuration of a benchmark.
  """
  let samples: USize
    """
    Total number of samples to be measured. (Default: 20)
    """
  let max_iterations: U64
    """
    Maximum number of iterations to execute per sample. (Default: 1_000_000_000)
    """
  let max_sample_time: U64
    """
    Maximum time to execute a sample in Nanoseconds. (Default: 100_000_000)
    """

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
