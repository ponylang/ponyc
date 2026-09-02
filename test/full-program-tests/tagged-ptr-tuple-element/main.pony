use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    // Tuple with a taggable union element
    let t: ((String, U32) | None) = ("hello", U32(42))

    let tuple_ok = match t
    | (let s: String, let n: U32) => (s == "hello") and (n == 42)
    else false
    end

    // Tuple with Bool union element
    let t2: ((U32, Bool) | None) = (U32(99), true)

    let tuple2_ok = match t2
    | (let n: U32, let b: Bool) => (n == 99) and b
    else false
    end

    if tuple_ok and tuple2_ok then
      @pony_exitcode(1)
    end
