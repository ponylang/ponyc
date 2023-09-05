use "lib:codegen-trace-additional"
use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_has_object[Bool](obj_map: Pointer[None], obj: Any tag)
use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)
use @pony_exitcode[None](code: I32)

class C

interface tag I
  be trace(x: (C iso, C iso))

actor Main
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    let i: I = this
    i.trace((C, C))
    map_before = @gc_local_snapshot(this)

  be trace(x: Any iso) =>
    let y = consume ref x
    match y
    | (let c1: C, let c2: C) =>
      let map_after = @gc_local(this)
      let ok = @objectmap_has_object_rc(map_before, y, USize(1)) and
        @objectmap_has_object_rc(map_after, y, USize(0)) and
        @objectmap_has_object_rc(map_before, c1, USize(1)) and
        @objectmap_has_object_rc(map_after, c1, USize(0)) and
        @objectmap_has_object_rc(map_before, c2, USize(1)) and
        @objectmap_has_object_rc(map_after, c2, USize(0))
      @gc_local_snapshot_destroy(map_before)
      @pony_exitcode(I32(if ok then 1 else 0 end))
    end
