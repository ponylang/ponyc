use "time"

primitive PropertyParamsDefaults
  """
  Default values for property test health-check thresholds.
  """
  fun max_filter_discard_ratio(): F64 => 10.0
  fun max_choice_sequence_size(): USize => 10_000
  fun max_sample_nanos(): U64 => 1_000_000_000

class val PropertyParams is Stringable
  """
  Parameters controlling property test execution.
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
  A property that consumes one generated argument of type `T`.

  Implement `name()`, `gen()`, and `property()`. Register with
  `test(PropertyTest[T](MyProperty))`.
  """
  fun name(): String

  fun params(): PropertyParams =>
    PropertyParams

  fun gen(): Generator[T]

  fun ref property(arg1: T, h: TestHelper) ?

trait Property2[T1, T2] is Property[(T1, T2)]
  """
  A property with two generated arguments.
  """

  fun gen1(): Generator[T1]

  fun gen2(): Generator[T2]

  fun gen(): Generator[(T1, T2)] =>
    Generators.zip2[T1, T2](
      gen1(),
      gen2())

  fun ref property(arg1: (T1, T2), h: TestHelper) ? =>
    (let x, let y) = consume arg1
    property2(consume x, consume y, h)?

  fun ref property2(arg1: T1, arg2: T2, h: TestHelper) ?

trait Property3[T1, T2, T3] is Property[(T1, T2, T3)]
  """
  A property with three generated arguments.
  """

  fun gen1(): Generator[T1]

  fun gen2(): Generator[T2]

  fun gen3(): Generator[T3]

  fun gen(): Generator[(T1, T2, T3)] =>
    Generators.zip3[T1, T2, T3](
      gen1(),
      gen2(),
      gen3())

  fun ref property(arg1: (T1, T2, T3), h: TestHelper) ? =>
    (let x, let y, let z) = consume arg1
    property3(consume x, consume y, consume z, h)?

  fun ref property3(arg1: T1, arg2: T2, arg3: T3, h: TestHelper) ?

trait Property4[T1, T2, T3, T4] is Property[(T1, T2, T3, T4)]
  """
  A property with four generated arguments.
  """

  fun gen1(): Generator[T1]

  fun gen2(): Generator[T2]

  fun gen3(): Generator[T3]

  fun gen4(): Generator[T4]

  fun gen(): Generator[(T1, T2, T3, T4)] =>
    Generators.zip4[T1, T2, T3, T4](
      gen1(),
      gen2(),
      gen3(),
      gen4())

  fun ref property(arg1: (T1, T2, T3, T4), h: TestHelper) ? =>
    (let x1, let x2, let x3, let x4) = consume arg1
    property4(consume x1, consume x2, consume x3, consume x4, h)?

  fun ref property4(
    arg1: T1,
    arg2: T2,
    arg3: T3,
    arg4: T4,
    h: TestHelper)
    ?
