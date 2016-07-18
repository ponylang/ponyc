class MapFn[A, B] is Iterator[B]
  """
  Take an iterator and a function and return an iterator where each
  item's value is the application of the given function to the value
  in the original iterator.

  ## Example program

  Double all of the numbers given as command line arguments.

  ```pony
  use "itertools"

  actor Main
    new create(env: Env) =>
      let fn = lambda (s: String): I32 => try s.i32() * 2 else 0 end end
      for x in MapFn[String, I32](env.args.slice(1).values(), fn) do
        env.out.print(x.string())
      end
  ```
  """
  let _iter: Iterator[A]
  let _f: {(A!): B ?} box

  new create(iter: Iterator[A], f: {(A!): B ?} box) =>
    _iter = iter
    _f = f

  fun ref has_next(): Bool =>
    _iter.has_next()

  fun ref next(): B ? =>
    try
      _f(_iter.next())
    else
      if _iter.has_next() then
        next()
      else
        error
      end
    end
