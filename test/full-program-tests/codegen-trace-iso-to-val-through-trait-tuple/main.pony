use "lib:codegen-trace-additional"

use @raw_cast[B ref](obj: B tag)
use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)
use @pony_exitcode[None](code: I32)

class A

class B
  let a: A = A

trait tag Receiver
  be receive(t: (B iso, B iso))

actor Main is Receiver
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    let r: Receiver = this
    r.receive((recover B end, recover B end))
    map_before = @gc_local_snapshot(this)

  be receive(t: (B val, B val)) =>
    (let b1, let b2) = t
    let b1' = @raw_cast(b1)
    let b2' = @raw_cast(b2)
    let map_after = @gc_local(this)
    let ok = @objectmap_has_object_rc(map_before, b1', USize(1)) and
      @objectmap_has_object_rc(map_before, b1'.a, USize(1)) and
      @objectmap_has_object_rc(map_before, b2', USize(1)) and
      @objectmap_has_object_rc(map_before, b2'.a, USize(1)) and
      @objectmap_has_object_rc(map_after, b1', USize(0)) and
      @objectmap_has_object_rc(map_after, b1'.a, USize(0)) and
      @objectmap_has_object_rc(map_after, b2', USize(0)) and
      @objectmap_has_object_rc(map_after, b2'.a, USize(0))
    @gc_local_snapshot_destroy(map_before)
    @pony_exitcode(I32(if ok then 1 else 0 end))
