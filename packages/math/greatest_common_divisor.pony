primitive GreatestCommonDivisor
  """
  Get greatest common divisor of x and y.

  Providing 0 will result in an error.

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
    either is zero.
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

    x'
