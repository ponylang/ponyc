trait Property2[T1, T2] is Property[(T1, T2)]
  """
  A property with two generated arguments.
  """

  fun gen1(): Generator[T1]
    """
    The generator for the first argument.
    """

  fun gen2(): Generator[T2]
    """
    The generator for the second argument.
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
    The property to verify for each generated sample.
    """
