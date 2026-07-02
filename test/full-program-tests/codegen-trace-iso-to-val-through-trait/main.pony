use "lib:codegen-trace-additional"

use @raw_cast[B ref](obj: B tag)
use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_has_object[Bool](obj_map: Pointer[None], obj: Any tag)
use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)
use @pony_exitcode[None](code: I32)

class A

class B
  let a: A = A

trait tag Receiver
  be receive(b: B iso)

actor Main is Receiver
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    // Call through a trait-typed variable to exercise the forwarding path.
    let r: Receiver = this
    r.receive(recover B end)
    map_before = @gc_local_snapshot(this)

  be receive(b: B val) =>
    let b' = @raw_cast(b)
    let map_after = @gc_local(this)
    // The sender traces b as MUTABLE (iso from the trait), which recurses into
    // B's fields, incrementing a.rc. The receiver's dispatch case must also
    // recurse to decrement a.rc. Without the fix, the dispatch traces as
    // IMMUTABLE (val from the concrete method), skipping field recursion when
    // might_reference_actor is false, leaving a.rc stuck at 1.
    let ok = @objectmap_has_object_rc(map_before, b', USize(1)) and
      @objectmap_has_object_rc(map_before, b'.a, USize(1)) and
      @objectmap_has_object_rc(map_after, b', USize(0)) and
      @objectmap_has_object_rc(map_after, b'.a, USize(0))
    @gc_local_snapshot_destroy(map_before)
    @pony_exitcode(I32(if ok then 1 else 0 end))
