use "lib:codegen-trace-additional"

use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_size[USize](obj_map: Pointer[None])
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])
use @pony_exitcode[None](code: I32)

actor Main
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    // Trigger GC to remove env from the object map.
    @pony_triggergc(@pony_ctx())
    test()

  be test() =>
    map_before = @gc_local_snapshot(this)
    trace(42, None)

  be trace(x: U32, y: None) =>
    let map_after = @gc_local(this)
    let size_before = @objectmap_size(map_before)
    let size_after = @objectmap_size(map_after)
         // Both maps should be empty.
    let ok = (size_before == 0) and (size_after == 0)
    @gc_local_snapshot_destroy(map_before)
    @pony_exitcode(I32(if ok then 1 else 0 end))
