// Regression test for a codegen alignment bug.
//
// An object with a U128 field requires 16-byte alignment, but the runtime
// allocator functions were declared to LLVM as returning only 8-byte-aligned
// memory. When the optimizer promoted such a heap allocation to the stack it
// produced an 8-byte-aligned slot, and the aligned 128-bit store into that
// slot faulted at runtime in optimized (non-debug) builds.

class val Dec
  let _m: U128
  new val create(m: U128) => _m = m
  fun value(): U128 => _m
  fun gt(o: Dec): Bool => _m > o._m

actor Worker
  let _env: Env
  new create(env: Env) => _env = env
  be go() =>
    var acc = Dec(0)
    var i: U128 = 0
    while i < 50 do
      let d = Dec(i * 7)
      if d.gt(acc) then acc = d end
      i = i + 1
    end
    if acc.value() != 343 then
      _env.exitcode(1)
    end

actor Main
  new create(env: Env) =>
    Worker(env).go()
