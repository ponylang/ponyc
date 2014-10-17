class Fibonacci[A: (Integer[A] & Unsigned)] is Iterator[A]
  var _last: A = 0
  var _next: A = 0
  var _uber_next: A = 1

  fun box apply(n: U8): A =>
    if n == 0 then
      return 0
    elseif n == 1 then
      return 1
    end

    let j = n / 2
    let fib_j = apply(j)
    let fib_i = apply(j - 1)

    if (n % 2) == 0 then
      return fib_j * (fib_j + (2 * fib_i))
    elseif (n % 4) == 1 then
      return (((2 * fib_j) + fib_i) * ((2 * fib_j) - fib_i)) + 2
    end

    return (((2 * fib_j) + fib_i) * ((2 * fib_j) - fib_i)) - 2

  //The generator stops on overflow.
  fun box has_next(): Bool => _last <= _next

  fun ref next(): A => _last = _next = _uber_next = _next + _uber_next ; _last
