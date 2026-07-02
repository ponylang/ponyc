// Regression test for partial/non-partial vtable slot segregation through a
// union receiver. `(A | B)` is a subtype of the partial trait `G` because both
// members implement `get`; `A` does so non-partially (its partial slot is a
// forwarding wrapper) and `B` partially (its own partial slot). Dispatching
// `get` on the union must reach the right slot on each concrete, and B's error
// must propagate through its partial slot.
trait G
  fun get(x: U32): U32 ?

class A is G
  fun get(x: U32): U32 => x + 10

class B is G
  fun get(x: U32): U32 ? =>
    if x == 99 then error end
    x + 20

actor Main
  new create(env: Env) =>
    var total: U32 = 0
    let xs: Array[(A | B)] = [A; B]
    for x in xs.values() do
      total = total + (try x.get(0)? else U32(0) end)
    end

    // Error path through the partial slot: B.get(99) raises and must be caught.
    let b: (A | B) = B
    let caught = try b.get(99)?; false else true end

    if (total != 30) or (not caught) then env.exitcode(1) end
