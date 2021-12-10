use "lib:codegen-trace-additional"
use @gc_local[Pointer[None]](target: Main)
use @objectmap_has_object[Bool](obj_map: Pointer[None], obj: Bar tag)
use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)
use @pony_exitcode[None](code: I32)

class Foo

struct Bar
  let f: Foo = Foo

class WrapBar
  let s: Bar = Bar

actor Main
  new create(env: Env) =>
    trace(recover WrapBar end)

  be trace(w: WrapBar iso) =>
    let x = consume ref w
    let map = @gc_local(this)
    let ok = @objectmap_has_object(map, x.s) and
      @objectmap_has_object_rc(map, x.s.f, 0)
    @pony_exitcode(I32(if ok then 1 else 0 end))
