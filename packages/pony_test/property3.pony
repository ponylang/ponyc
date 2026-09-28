trait Property3[T1, T2, T3] is Property[(T1, T2, T3)]
  """
  A property with three generated arguments.
  """

  fun gen1(): Generator[T1]
    """
    The generator for the first argument.
    """

  fun gen2(): Generator[T2]
    """
    The generator for the second argument.
    """

  fun gen3(): Generator[T3]
    """
    The generator for the third argument.
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
    The property to verify for each generated sample.
    """
