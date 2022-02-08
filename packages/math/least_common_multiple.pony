primitive LeastCommonMultiple
  """
  Get the least common multiple of x and y where both x and y >= 1.

  Providing 0 or numbers that overflow the integer width will result in an
  error.
  """
  fun apply[A: (Integer[A] val & Unsigned)](x: A, y: A): A ? =>
    (x *? y) / GreatestCommonDivisor[A](x, y)?
