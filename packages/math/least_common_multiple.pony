primitive LeastCommonMultiple
  """
  Get the least common multiple of x and y where both x and y >= 1.

  Providing 0 or numbers that overflow the integer width will result in an
  error.

  Example usage:

  ```pony
  use "math"

  actor Main
    new create(env: Env) =>
      try
        let lcm = LeastCommonMultiple[U64](10, 20)?
        env.out.print(lcm.string())
      else
        env.out.print("No LCM")
      end
  ```
  """
  fun apply[A: (Integer[A] val & Unsigned)](x: A, y: A): A ? =>
    (x / GreatestCommonDivisor[A](x, y)?) *? y
