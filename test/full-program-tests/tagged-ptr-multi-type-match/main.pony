use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    // Match discriminating between multiple tagged types in a single union.
    let a: (U8 | I8 | U32 | Bool | None) = U8(200)
    let b: (U8 | I8 | U32 | Bool | None) = U32(70000)
    let c: (U8 | I8 | U32 | Bool | None) = true
    let d: (U8 | I8 | U32 | Bool | None) = None
    let e: (U8 | I8 | U32 | Bool | None) = I8(-42)

    let a_ok = match a
    | let v: U8 => v == 200
    else false
    end

    let b_ok = match b
    | let v: U32 => v == 70000
    else false
    end

    let c_ok = match c
    | let v: Bool => v == true
    else false
    end

    let d_ok = match d
    | None => true
    else false
    end

    let e_ok = match e
    | let v: I8 => v == -42
    else false
    end

    if a_ok and b_ok and c_ok and d_ok and e_ok then
      @pony_exitcode(1)
    end
