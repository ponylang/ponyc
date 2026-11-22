use "lib:codegen-trace-additional"

use @gc_local[Pointer[None]](target: Main)
use @objectmap_has_object_rc[Bool](
  map: Pointer[None], obj: Any tag, rc: USize)
use @objectmap_is_immutable[Bool](map: Pointer[None], obj: Any tag)

class val Leaf
  let value: U32
  new val create(value': U32) => value = value'

actor Main
  let _env: Env
  var _sent: Bool = false

  new create(env: Env) =>
    _env = env
    let a = Leaf(1)
    let b = Leaf(2)
    let a_tag: Leaf tag = a
    let b_tag: Leaf tag = b
    receive(a_tag, a, b, b_tag)
    _sent = _counted(a, 1) and _counted(b, 1)

  fun _counted(leaf: Leaf tag, count: USize): Bool =>
    let map = @gc_local(this)
    @objectmap_has_object_rc(map, leaf, count) and @objectmap_is_immutable(
      map, leaf)

  be receive(a_tag: Leaf tag, a: Leaf val, b: Leaf val, b_tag: Leaf tag) =>
    if _sent and (a_tag is a) and (b_tag is b) and _counted(a, 0)
      and _counted(b, 0)
    then
      _env.exitcode(1)
    end
