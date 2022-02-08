primitive LeastCommonMultiple
  """
  Get the least common multiple of x and y
  """
  fun apply(x: USize, y: USize): USize ? =>
    (x*y)/GreatestCommonDivisor(x, y)?
