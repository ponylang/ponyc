// Regression test for https://github.com/ponylang/ponyc/issues/5462
//
// A class with a U128 field that does not escape gets promoted from the heap to
// a stack alloca by the HeapToStack optimisation pass. U128 has 16-byte
// alignment on x86-64, so the field sits at offset 16 in the object. The pass
// used to give the alloca only the pointer alignment (8 bytes), so the field
// was under-aligned and the optimiser's aligned vector store (vmovaps) faulted
// at runtime. This only manifests in an optimised (non-debug) build, so the
// release pass of the full-program test suite is what exercises it.

class val Dec
  let _m: U128

  new val create(m: U128) =>
    _m = m

  fun value(): U128 =>
    _m

  fun gt(o: Dec): Bool =>
    _m > o._m

actor W
  let _e: Env

  new create(e: Env) =>
    _e = e

  be go() =>
    var acc = Dec(0)
    var i: U128 = 0
    while i < 50 do
      let d = Dec(i * 7)
      if d.gt(acc) then acc = d end
      i = i + 1
    end
    if acc.value() != 343 then
      _e.exitcode(1)
    end

actor Main
  new create(e: Env) =>
    W(e).go()
