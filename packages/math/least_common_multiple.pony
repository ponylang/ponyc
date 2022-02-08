primitive LeastCommonMultiple
  """
  Get the least common multiple of x and y
  """
  fun apply[A: (Integer[A] val & Unsigned)](x: A, y: A): A ? =>
    (x *? y) / GreatestCommonDivisor[A](x, y)?
