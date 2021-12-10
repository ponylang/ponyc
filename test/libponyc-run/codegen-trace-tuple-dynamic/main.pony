use "lib:codegen-trace-additional"
use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)
use @pony_exitcode[None](code: I32)

class A

actor Main
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    let t: ((A iso, A iso) | A val) = (recover A end, recover A end)
    trace(consume t)
    map_before = @gc_local_snapshot(this)

  be trace(t: ((A iso, A iso) | A val)) =>
    let t' = t
    match consume t
    | (let t1: A, let t2: A) =>
      let map_after = @gc_local(this)
      let ok = @objectmap_has_object_rc(map_before, t', USize(1)) and
      @objectmap_has_object_rc(map_before, t1, USize(1)) and
      @objectmap_has_object_rc(map_before, t2, USize(1)) and
      @objectmap_has_object_rc(map_after, t', USize(0)) and
      @objectmap_has_object_rc(map_after, t1, USize(0)) and
      @objectmap_has_object_rc(map_after, t2, USize(0))
      @gc_local_snapshot_destroy(map_before)
      @pony_exitcode(I32(if ok then 1 else 0 end))
    end
