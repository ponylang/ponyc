primitive GreatestCommonDivisor
  """
  Get greatest common divisor of x and y.

  Errors when either argument is zero or when the result cannot be
  represented (e.g. `gcd(I8.min_value(), I8.min_value())`).

  Example usage:

  ```pony
  use "math"

  actor Main
    new create(env: Env) =>
      try
        let gcd = GreatestCommonDivisor[I64](10, 20)?
        env.out.print(gcd.string())
      else
        env.out.print("No GCD")
      end
  ```
  """
  fun apply[A: Integer[A] val](x: A, y: A): A ? =>
    """
    Returns the greatest common divisor of `x` and `y`. Errors if
    either is zero or if the result cannot be represented in `A`.
    """
    let zero = A.from[U8](0)
    if (x == zero) or (y == zero) then
      error
    end

    var x': A = x
    var y': A = y

    while y' != zero do
      let z = y'
      y' = x' % y'
      x' = z
    end

    let result = if x' < zero then -x' else x' end
    if result < zero then error end
    result
