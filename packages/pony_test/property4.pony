trait Property4[T1, T2, T3, T4] is Property[(T1, T2, T3, T4)]
  """
  A property with four generated arguments.
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

  fun gen4(): Generator[T4]
    """
    The generator for the fourth argument.
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

  fun ref property4(
    arg1: T1,
    arg2: T2,
    arg3: T3,
    arg4: T4,
    h: PropertyHelper)
    ?
    """
    The property to verify for each generated sample.
    """
