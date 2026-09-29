use "time"

primitive PropertyParamsDefaults
  """
  Health-check thresholds used when PropertyParams fields are not set.
  """
  fun max_filter_discard_ratio(): F64 => 10.0
  fun max_choice_sequence_size(): USize => 10_000
  fun max_sample_nanos(): U64 => 1_000_000_000

class val PropertyParams is Stringable
  """
  Configuration for a property test: sample count, seed, shrink budget,
  health-check limits, and regression persistence.

  - `seed` — PRNG seed. Defaults to `Time.millis()` for variety across
    runs. Fix to a constant to reproduce a failure.
  - `num_samples` — how many generated samples to run.
  - `max_shrink_reductions` — budget for shrink candidates per failure.
  - `max_generator_retries` — consecutive generator errors before the
    property is abandoned.
  - `timeout` — default timeout in nanoseconds, available as a convenience
    for passing to `h.long_test(params().timeout)` in async properties.
  - `max_filter_discard_ratio` — ratio of discarded to accepted filter
    draws before the health check fails.
  - `max_choice_sequence_size` — total random draws per sample before
    the health check fails.
  - `max_sample_nanos` — wall-clock nanoseconds per sample before the
    health check warns.
  - `regression_db` — whether to persist and replay failing choice
    sequences via the `.ponytest/` directory.
  """
  let seed: U64
  let num_samples: USize
  let max_shrink_reductions: USize
  let max_generator_retries: USize
  let timeout: U64
  let max_filter_discard_ratio: F64
  let max_choice_sequence_size: USize
  let max_sample_nanos: U64
  let regression_db: Bool

  new val create(
    num_samples': USize = 100,
    seed': U64 = Time.millis(),
    max_shrink_reductions': USize = 100,
    max_generator_retries': USize = 5,
    timeout': U64 = 60_000_000_000,
    max_filter_discard_ratio': F64 =
      PropertyParamsDefaults.max_filter_discard_ratio(),
    max_choice_sequence_size': USize =
      PropertyParamsDefaults.max_choice_sequence_size(),
    max_sample_nanos': U64 =
      PropertyParamsDefaults.max_sample_nanos(),
    regression_db': Bool = true)
  =>
    num_samples = num_samples'
    seed = seed'
    max_shrink_reductions = max_shrink_reductions'
    max_generator_retries = max_generator_retries'
    timeout = timeout'
    max_filter_discard_ratio = max_filter_discard_ratio'
    max_choice_sequence_size = max_choice_sequence_size'
    max_sample_nanos = max_sample_nanos'
    regression_db = regression_db'

  fun string(): String iso^ =>
    recover
      let s = String()
        .> append("Params(seed=")
        .> append(seed.string())
      if max_filter_discard_ratio !=
        PropertyParamsDefaults.max_filter_discard_ratio()
      then
        s
          .> append(", max_filter_discard_ratio=")
          .> append(max_filter_discard_ratio.string())
      end
      if max_choice_sequence_size !=
        PropertyParamsDefaults.max_choice_sequence_size()
      then
        s
          .> append(", max_choice_sequence_size=")
          .> append(max_choice_sequence_size.string())
      end
      if max_sample_nanos !=
        PropertyParamsDefaults.max_sample_nanos()
      then
        s
          .> append(", max_sample_nanos=")
          .> append(max_sample_nanos.string())
      end
      s .> append(")")
      s
    end

trait Property[T]
  """
  A property test over one generated argument of type `T`.

  Implement `name()`, `gen()`, and `property()`. Register with
  `test.property(MyProperty)`.
  """
  fun name(): String
    """
    The test name.
    """

  fun params(): PropertyParams =>
    PropertyParams

  fun gen(): Generator[T]
    """
    The generator for this property's input type.
    """

  fun ref property(arg1: T, h: PropertyHelper) ?
    """
    The property to verify for each generated sample.
    """
