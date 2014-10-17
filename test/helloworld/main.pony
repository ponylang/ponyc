class Fibonacci is Iterator[U64]
  var _first: U64 = U64(0)
  var _second: U64 = U64(1)

  fun box get(n: U64): U64 =>
    if n <= U64(1) then return n end

    let j = n / U64(2)
    let k = get(j)
    let l = get(j - U64(1))

    if (n % U64(2)) == U64(0) then
      return k * (k + (U64(2) * l))
    elseif (n % U64(4)) == U64(1) then
      return (((U64(2) * k) + l) * ((U64(2) * k) - l)) + U64(2)
    end

    return (((U64(2) * k) + l) * ((U64(2) * k) - l)) - U64(2)

  //Fibonacci is an infinite sequence
  //User has to break out of the loop.
  fun box has_next(): Bool => true

  fun ref next(): U64 =>
    var n = _first + _second
    _first = _second
    _second = n
    n

actor Main
  new create(env: Env) =>
    var str = String

    for s in env.args.values() do
      str.append(s)
      str.append(" ")
    end

    env.stdout.print((str + "!").lower())

    for f in Fibonacci do
      env.stdout.print(f.string())
    end
