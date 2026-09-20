use "time"

primitive PropertyParamsDefaults
  """
  Default thresholds for health check warnings.
  """
  fun max_filter_discard_ratio(): F64 => 10.0
  fun max_choice_sequence_size(): USize => 10_000
  fun max_sample_nanos(): U64 => 1_000_000_000

class val PropertyParams is Stringable
  """
  Parameters to control property execution.

  * seed: the seed for the source of Randomness
  * num_samples: the number of samples to produce from the property generator
  * max_shrink_reductions: the maximum number of shrink reductions to accept
  * max_generator_retries: the maximum number of retries to do if a generator
    fails to generate a sample
  * timeout: the timeout for the PonyTest runner, in nanoseconds
  * async: if true the property is expected to finish asynchronously by calling
    `PropertyHelper.complete(...)`
  * max_filter_discard_ratio: the maximum allowed ratio of filter discards to
    accepts across the run. 10.0 allows up to 10 rejections per accepted
    value (~91% rejection rate). 0 or negative disables the check.
  * max_choice_sequence_size: the maximum number of choice-sequence entries
    allowed in any single sample before a warning is logged. 0 disables the
    check.
  * max_sample_nanos: the maximum wall-clock duration in nanoseconds allowed
    for any single sample before a warning is logged. 0 disables the check.
  """
  let seed: U64
  let num_samples: USize
  let max_shrink_reductions: USize
  let max_generator_retries: USize
  let timeout: U64
  let async: Bool
  let max_filter_discard_ratio: F64
  let max_choice_sequence_size: USize
  let max_sample_nanos: U64

  new val create(
    num_samples': USize = 100,
    seed': U64 = Time.millis(),
    max_shrink_reductions': USize = 100,
    max_generator_retries': USize = 5,
    timeout': U64 = 60_000_000_000,
    async': Bool = false,
    max_filter_discard_ratio': F64 =
      PropertyParamsDefaults.max_filter_discard_ratio(),
    max_choice_sequence_size': USize =
      PropertyParamsDefaults.max_choice_sequence_size(),
    max_sample_nanos': U64 =
      PropertyParamsDefaults.max_sample_nanos())
  =>
    num_samples = num_samples'
    seed = seed'
    max_shrink_reductions = max_shrink_reductions'
    max_generator_retries = max_generator_retries'
    timeout = timeout'
    async = async'
    max_filter_discard_ratio = max_filter_discard_ratio'
    max_choice_sequence_size = max_choice_sequence_size'
    max_sample_nanos = max_sample_nanos'

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

trait Property1[T]
  """
  A property that consumes 1 argument of type `T`.

  A property is defined by a [Generator](pony_check-Generator.md), returned by
  the [`gen()`](pony_check-Property1.md#gen) method, and a
  [`property`](pony_check-Property1#property) method that consumes the
  generators' output and verifies a custom property with the help of a
  [PropertyHelper](pony_check-PropertyHelper.md).

  A property is verified if no failed assertion on
  [PropertyHelper](pony_check-PropertyHelper.md) has been
  reported for all the samples it consumed.

  The property execution can be customized by returning a custom
  [PropertyParams](pony_check-PropertyParams.md) from the
  [`params()`](pony_check-Property1.md#params) method.

  The [`gen()`](pony_check-Property1.md#gen) method is called exactly once to
  instantiate the generator.
  The generator produces
  [PropertyParams.num_samples](pony_check-PropertyParams.md#num_samples)
  samples and each is passed to the
  [property](pony_check-Property1.md#property) method for verification.

  If the property did not verify, the framework automatically shrinks the
  failing sample by replaying the generator against mutated choice sequences.
  The smallest counterexample found is reported to the user.

  A [Property1](pony_check-Property1.md) can be run with
  [Ponytest](pony_test--index.md).
  To that end it needs to be wrapped into a
  [Property1UnitTest](pony_check-Property1UnitTest.md).
  """
  fun name(): String
    """
    The name of the property used for reporting during execution.
    """

  fun params(): PropertyParams =>
    """
    Returns parameters to customize execution of this Property.
    """
    PropertyParams

  fun gen(): Generator[T]
    """
    The [Generator](pony_check-Generator.md) used to produce samples to verify.
    """

  fun ref property(arg1: T, h: PropertyHelper) ?
    """
    A method verifying that a certain property holds for all given `arg1`
    with the help of [PropertyHelper](pony_check-PropertyHelper.md) `h`.
    """

trait Property2[T1, T2] is Property1[(T1, T2)]
  """
  A property with two generated arguments.
  """

  fun gen1(): Generator[T1]
    """
    The Generator for the first argument to your `property2`.
    """

  fun gen2(): Generator[T2]
    """
    The Generator for the second argument to your `property2`.
    """

  fun gen(): Generator[(T1, T2)] =>
    Generators.zip2[T1, T2](
      gen1(),
      gen2())

  fun ref property(arg1: (T1, T2), h: PropertyHelper) ? =>
    (let x, let y) = consume arg1
    property2(consume x, consume y, h)?

  fun ref property2(arg1: T1, arg2: T2, h: PropertyHelper) ?
    """
    A method verifying that a certain property holds for all given
    `arg1` and `arg2`
    with the help of [PropertyHelper](pony_check-PropertyHelper.md) `h`.
    """

trait Property3[T1, T2, T3] is Property1[(T1, T2, T3)]
  """
  A property with three generated arguments.
  """

  fun gen1(): Generator[T1]
    """
    The Generator for the first argument to your `property3` method.
    """

  fun gen2(): Generator[T2]
    """
    The Generator for the second argument to your `property3` method.
    """

  fun gen3(): Generator[T3]
    """
    The Generator for the third argument to your `property3` method.
    """

  fun gen(): Generator[(T1, T2, T3)] =>
    Generators.zip3[T1, T2, T3](
      gen1(),
      gen2(),
      gen3())

  fun ref property(arg1: (T1, T2, T3), h: PropertyHelper) ? =>
    (let x, let y, let z) = consume arg1
    property3(consume x, consume y, consume z, h)?

  fun ref property3(arg1: T1, arg2: T2, arg3: T3, h: PropertyHelper) ?
    """
    A method verifying that a certain property holds for all given
    `arg1`,`arg2`, and `arg3`
    with the help of [PropertyHelper](pony_check-PropertyHelper.md) `h`.
    """

trait Property4[T1, T2, T3, T4] is Property1[(T1, T2, T3, T4)]
  """
  A property with four generated arguments.
  """

  fun gen1(): Generator[T1]
    """
    The Generator for the first argument to your `property4` method.
    """

  fun gen2(): Generator[T2]
    """
    The Generator for the second argument to your `property4` method.
    """

  fun gen3(): Generator[T3]
    """
    The Generator for the third argument to your `property4` method.
    """

  fun gen4(): Generator[T4]
    """
    The Generator for the fourth argument to your `property4` method.
    """

  fun gen(): Generator[(T1, T2, T3, T4)] =>
    Generators.zip4[T1, T2, T3, T4](
      gen1(),
      gen2(),
      gen3(),
      gen4())

  fun ref property(arg1: (T1, T2, T3, T4), h: PropertyHelper) ? =>
    (let x1, let x2, let x3, let x4) = consume arg1
    property4(consume x1, consume x2, consume x3, consume x4, h)?

  fun ref property4(arg1: T1, arg2: T2, arg3: T3, arg4: T4, h: PropertyHelper) ?
    """
    A method verifying that a certain property holds for all given
    `arg1`, `arg2`, `arg3`, and `arg4`
    with the help of [PropertyHelper](pony_check-PropertyHelper.md) `h`.
    """

