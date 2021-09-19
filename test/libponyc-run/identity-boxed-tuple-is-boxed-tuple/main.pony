use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let t1_a: (Any | (U32, U32) | (U64, U64)) = (U32(0), U32(0))
    let t1_b: (Any | (U32, U32) | (U64, U64)) = (U32(0), U32(0))
    let t2: (Any | (U32, U32) | (U64, U64)) = (U32(1), U32(0))
    let t3: (Any | (U32, U32) | (U64, U64)) = (U64(0), U64(0))
    if (t1_a is t1_a) and (t1_a is t1_b) and (t1_a isnt t2) and
      (t1_a isnt t3)
    then
      @pony_exitcode(I32(1))
    end
