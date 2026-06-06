use "lib:regression-1118-additional"
use @pony_exitcode[None](code: I32)
use @gc_foreign[Pointer[None]](recv: Receiver)
use @actormap_has_actor[Bool](map: Pointer[None], target: Target tag)
use @actormap_actor_rc[USize](map: Pointer[None], target: Target tag)

// Regression test for https://github.com/ponylang/ponyc/issues/1118.
//
// #1118 was caused by an unsafe garbage collection optimization: an immutable
// object sent between actors was not traced, on the assumption that immutable
// roots don't need per-object reference counting. When such an object contained
// a reference to an actor, the actor's foreign reference count was never
// incremented, so the actor could be reaped while a live reference to it still
// existed inside the object. That produced an intermittent use-after-free
// segfault. The fix (#4256) traces any type the compiler marks as
// `might_reference_actor`.
//
// Rather than racing to reproduce the crash with a stress loop (which is timing
// dependent and times out on slow CI hardware), this test asserts the invariant
// the fix restores: when an immutable object carrying an actor reference
// crosses an inter-actor send, the reference is counted. `Receiver` sends back
// exit code 1 only when the carried actor is present in its foreign actormap
// with the expected reference count; with the bug present the actor is absent
// and the exit code is 0.

actor Target

class val Holder
  let t: Target
  new val create(t': Target) => t = t'

actor Receiver
  be take(h: Holder) =>
    // `h.t` reached this actor only inside the immutable `Holder`. If it was
    // traced on send, `Target` is now in our foreign actormap with rc == 1.
    let map = @gc_foreign(this)
    let ok = @actormap_has_actor(map, h.t) and
      (@actormap_actor_rc(map, h.t) == 1)
    @pony_exitcode(I32(if ok then 1 else 0 end))

actor Main
  new create(env: Env) =>
    let target = Target
    let h = recover val Holder(target) end
    Receiver.take(consume h)
