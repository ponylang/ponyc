use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let u32_0_a: (Any | (U32, U32)) = U32(0)
    let u32_0_b: (Any | (U32, U32)) = U32(0)
    let u32_1: (Any | (U32, U32)) = U32(1)
    let u64_0: (Any | (U32, U32)) = U64(0)
    if (u32_0_a is u32_0_a) and (u32_0_a is u32_0_b) and
      (u32_0_a isnt u32_1) and (u32_0_a isnt u64_0)
    then
      @pony_exitcode(1)
    end
