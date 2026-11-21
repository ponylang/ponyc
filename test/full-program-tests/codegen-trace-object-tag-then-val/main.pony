use "lib:codegen-trace-additional"

use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)
use @objectmap_has_object_immutable[Bool](obj_map: Pointer[None], obj: Any tag)
use @pony_exitcode[None](code: I32)

class A

class B
  let a: A = A
  // A field that can hold an actor makes B a type that might reference one,
  // so the GC traces B's fields when B is a val.
  let actor_field: (Main | None) = None

actor Main
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    let b: B val = recover B end
    trace(b, b)
    map_before = @gc_local_snapshot(this)

  be trace(t: B tag, b: B val) =>
    let map_after = @gc_local(this)
    let ok = @objectmap_has_object_rc(map_before, b, USize(1)) and
      @objectmap_has_object_immutable(map_before, b) and
      @objectmap_has_object_rc(map_before, b.a, USize(1)) and
      @objectmap_has_object_rc(map_after, b, USize(0)) and
      @objectmap_has_object_rc(map_after, b.a, USize(0))
    @gc_local_snapshot_destroy(map_before)
    @pony_exitcode(I32(if ok then 1 else 0 end))
