// Regression test for partial/non-partial vtable slot segregation.
//
// A single non-partial concrete method (`C.get`) is reached through a
// non-partial trait (`NP`, plain `T` return), a partial trait (`P`, which
// needs the `{T, i1}` calling convention), and their intersection. Partiality
// is encoded in the mangled name, so `NP.get` and `P.get` occupy separate
// vtable slots; the partial slot is filled by a forwarding wrapper that turns
// `C.get`'s `T` return into `{T, i1}`. Every dispatch below must land on the
// slot matching its own partiality and return the right value.
trait NP
  fun get(x: U32): U32

trait P
  fun get(x: U32): U32 ?

class C is (NP & P)
  fun get(x: U32): U32 => x + 7

actor Main
  new create(env: Env) =>
    let c: C = C
    let np: NP = c
    let p: P = c
    let both: (NP & P) = c

    let a = np.get(1)                      // non-partial trait slot
    let b = try p.get(2)? else U32(0) end  // partial trait slot (forwarding)
    let d = both.get(3)                     // intersection-receiver dispatch

    if (a != 8) or (b != 9) or (d != 10) then
      env.exitcode(1)
    end
