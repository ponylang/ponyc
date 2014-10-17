class Fibonacci[A: (Integer[A] & Unsigned)] is Iterator[A]
  var _first: A
  var _second: A

  new create() =>
    _first = 0
    _second = 1

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

  //Fibonacci is an infinite sequence
  //User has to break out of the loop.
  fun box has_next(): Bool => true

  fun ref next(): A => _first = _second = _first + _second
