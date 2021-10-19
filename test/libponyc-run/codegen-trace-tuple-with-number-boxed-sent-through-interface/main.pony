use "lib:codegen-trace-additional"
use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)
use @pony_exitcode[None](code: I32)

interface tag I
  be trace(x: (U32, U32))

actor Main
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    let i: I = this
    i.trace((42, 42))
    map_before = @gc_local_snapshot(this)

  be trace(x: (Any val, U32)) =>
    let map_after = @gc_local(this)
    let ok = @objectmap_has_object_rc(map_before, x._1, USize(1)) and
      @objectmap_has_object_rc(map_after, x._1, USize(0))
    @gc_local_snapshot_destroy(map_before)
    @pony_exitcode(I32(if ok then 1 else 0 end))
