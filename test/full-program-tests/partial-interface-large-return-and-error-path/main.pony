// Regression test for partial/non-partial vtable slot segregation on the two
// dimensions the small-scalar tests do not exercise: an ABI-sensitive
// (large/sret) return, and an actually-raised error propagating through a
// segregated partial slot.
//
// `f` returns a 3-tuple of `String val`. That aggregate is past the
// two-register threshold, so the partial calling convention (`{tuple, i1}`)
// and the non-partial one (`tuple`) disagree on the ABI — the exact shape that
// made the #5002/#5373 crash visible. `Big` is non-partial, so its partial
// slot is a forwarding wrapper that turns its `tuple` return into
// `{tuple, i1}`; `BigP` is partial and actually raises `error`. Both are
// dispatched through the partial interface `P`. The success values must come
// back intact through both slots, and `BigP`'s error must propagate through
// its partial slot and be caught.
trait P
  fun f(x: U32): (String val, String val, String val) ?

class Big is P
  fun f(x: U32): (String val, String val, String val) => ("a", "b", "c")

class BigP is P
  fun f(x: U32): (String val, String val, String val) ? =>
    if x == 99 then error end
    ("d", "e", "f")

actor Main
  new create(env: Env) =>
    var ok = true

    // Success path through the partial slot for both concretes: Big via its
    // forwarding wrapper, BigP via its own partial method.
    let items: Array[P] = [Big; BigP]
    for p in items.values() do
      try
        (let a, let b, let c) = p.f(0)?
        if (a.size() != 1) or (b.size() != 1) or (c.size() != 1) then
          ok = false
        end
      else
        ok = false  // f(0) must not error
      end
    end

    // Error path: BigP.f(99) raises; the error must travel through the
    // segregated partial slot and land in the else.
    let bp: P = BigP
    let caught =
      try
        bp.f(99)?
        false
      else
        true
      end
    if not caught then ok = false end

    if not ok then env.exitcode(1) end
