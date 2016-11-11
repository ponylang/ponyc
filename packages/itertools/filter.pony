primitive _None

class Filter[A] is Iterator[A]
  """
  Take an iterator and a predicate function and return an iterator
  that only returns items that match the predicate.

  ## Example program

  Only print command line arguments that are more than 3 characters long.

  ```pony
  use "itertools"

  actor Main
    new create(env: Env) =>
      let fn = {(s: String): Bool => x.size() > 3 }
      for x in Filter[String](env.args.slice(1).values(), fn) do
        env.out.print(x)
      end
  ```
  """
  let _fn: {(A!): Bool ?} val
  let _iter: Iterator[A^]
  var _next: (A | _None)

  new create(iter: Iterator[A^], fn: {(A!): Bool ?} val) =>
    _iter = iter
    _fn = fn
    _next = _None

  fun ref _find_next() =>
    try
      match _next
      | _None =>
        if _iter.has_next() then
          _next = _iter.next()
          if try not _fn(_next as A) else true end then
            _next = _None
            _find_next()
          end
        end
      end
    end

  fun ref has_next(): Bool =>
    match _next
    | _None =>
      if _iter.has_next() then
        _find_next()
        has_next()
      else
        false
      end
    else
      true
    end

  fun ref next(): A ? =>
    match _next
    | let v: A =>
      _next = _None
      consume v
    else
      if _iter.has_next() then
        _find_next()
        next()
      else
        error
      end
    end
