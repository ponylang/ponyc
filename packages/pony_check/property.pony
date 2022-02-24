use "time"

class val PropertyParams is Stringable
  """
  Parameters to control Property Execution.

  * seed: the seed for the source of Randomness
  * num_samples: the number of samples to produce from the property generator
  * max_shrink_rounds: the maximum rounds of shrinking to perform
  * max_generator_retries: the maximum number of retries to do if a generator fails to generate a sample
  * timeout: the timeout for the PonyTest runner, in nanoseconds
  * async: if true the property is expected to finish asynchronously by calling
    `PropertyHelper.complete(...)`
  """
  let seed: U64
  let num_samples: USize
  let max_shrink_rounds: USize
  let max_generator_retries: USize
  let timeout: U64
  let async: Bool

  new val create(
    num_samples': USize = 100,
    seed': U64 = Time.millis(),
    max_shrink_rounds': USize = 10,
    max_generator_retries': USize = 5,
    timeout': U64 = 60_000_000_000,
    async': Bool = false)
  =>
    num_samples = num_samples'
    seed = seed'
    max_shrink_rounds = max_shrink_rounds'
    max_generator_retries = max_generator_retries'
    timeout = timeout'
    async = async'

  fun string(): String iso^ =>
    recover
      String()
        .>append("Params(seed=")
        .>append(seed.string())
        .>append(")")
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

  If the property did not verify, the given sample is shrunken if the
  generator supports shrinking.
  The smallest shrunken sample will then be reported to the user.

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

