primitive GreatestCommonDivisor
  """
  Get greatest common divisor of x and y
  """
  fun apply(x: USize, y: USize): USize ? =>
    if (x == 0) or (y == 0) then
      error
    end

    var x': USize = x
    var y': USize = y

    while y' != 0 do
      let z = y'
      y' = x' % y'
      x' = z
    end

    x'
