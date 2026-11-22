use "lib:codegen-trace-additional"

use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_has_object_rc[Bool](
  obj_map: Pointer[None], obj: Any tag, rc: USize)
use @objectmap_is_immutable[Bool](obj_map: Pointer[None], obj: Any tag)
use @pony_exitcode[None](code: I32)

class A

class B
  let a: A = A
  // A val's fields are traced only if its type can reference an actor. This
  // field lets B reference one.
  let actor_field: (Main | None) = None

actor Main
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    let b: B val = recover B end
    trace(b, b, b)
    map_before = @gc_local_snapshot(this)

  // With a tag on each side of the val, a tag is traced first whichever order
  // the arguments are traced in.
  be trace(t: B tag, b: B val, t2: B tag) =>
    let map_after = @gc_local(this)
    let ok = @objectmap_has_object_rc(map_before, b, USize(1)) and
      @objectmap_is_immutable(map_before, b) and
      @objectmap_has_object_rc(map_before, b.a, USize(1)) and
      @objectmap_has_object_rc(map_after, b, USize(0)) and
      @objectmap_has_object_rc(map_after, b.a, USize(0))
    @gc_local_snapshot_destroy(map_before)
    @pony_exitcode(I32(if ok then 1 else 0 end))
