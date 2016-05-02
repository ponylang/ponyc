class Take[A] is Iterator[A]
  """
  Create an iterator from the original iterator that only returns a
  specified number of items.

  ## Example program

  Prints the numbers 10 through 12.

  ```pony
  use "itertools"

  actor Main
    new create(env: Env) =>
      let i1 = [as I32: 10, 11, 12, 13, 14]
    
      for x in Take[I32](i1.values(), 3) do
        env.out.print(x.string())
      end
  ```
  """
  var _countdown: USize
  let _i: Iterator[A]

  new create(i: Iterator[A], l: USize) =>
    _countdown = l
    _i = i

  fun ref has_next(): Bool =>
    (_countdown > 0) and _i.has_next()

  fun ref next(): A ? =>
    _countdown = _countdown - 1
    _i.next()
