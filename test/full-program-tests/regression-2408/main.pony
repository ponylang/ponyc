interface SafeOps[T]
  fun addc(t: T): (T, Bool)
  fun subc(t: T): (T, Bool)

class GenericSum[T: (Int & Integer[T] val & SafeOps[T])]
  fun _plus_safe(x: T, y: T): (T, Bool) =>
    x.addc(y)

  fun sum(x: T, y: T): T ? =>
    match _plus_safe(x, y)
    // In the issue 2408 ponyc < 0.26 was reporting this pattern to never match
    | (let res: T, false) => res
    | (_, true) => error
    else
      // this else is only needed because ponyc does not recognize
      // the match above as exhaustive
      error
    end

actor Main
  new create(env: Env) =>
    try
      GenericSum[U8].sum(1, 2)?
    end
