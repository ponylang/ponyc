"""
# Math package

Given the name `Math` for this package, you'd expect it have a broad and grand
scope. Surprise! Not currently. However, we do have the most useful of all
programming language math constructs: fibonacci!

People like to make fun of fibonacci but let's face it, no fibonacci, no
benchmarks. We hear from some of our engineer friends that math is very
important to programming, we call upon that particular class of engineer friends
to help us fill out this package with more maths than you can shake a stick at.
Btw, in case you are wondering, yes we can shake a stick at a lot of maths.
"""
class Fibonacci[A: (Integer[A] val & Unsigned) = U64] is Iterator[A]
  """
  Useful for microbenchmarks to impress your friends. Look y'all, Pony goes
  fast! We suppose if you are into Agile planning poker that you could also
  use this in conjunction with `Random` to assign User Story Points.
  """
  var _last: A = 0
  var _next: A = 0
  var _uber_next: A = 1

  fun apply(n: U8): A =>
    if n == 0 then
      0
    elseif n == 1 then
      1
    else
      let j = n / 2
      let fib_j = apply(j)
      let fib_i = apply(j - 1)

      if (n % 2) == 0 then
        fib_j * (fib_j + (fib_i * 2))
      elseif (n % 4) == 1 then
        (((fib_j * 2) + fib_i) * ((fib_j * 2) - fib_i)) + 2
      else
        (((fib_j * 2) + fib_i) * ((fib_j * 2) - fib_i)) - 2
      end
    end

  //The generator stops on overflow.
  fun has_next(): Bool => _last <= _next

  fun ref next(): A => _last = _next = _uber_next = _next + _uber_next ; _last
