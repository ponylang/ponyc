use "lib:codegen-trace-additional"

use @gc_foreign_has_object[Bool](target: Main, obj: USize)
use @gc_heap_next_gc[USize](target: Main)
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])
use @pony_exitcode[None](code: I32)

class A

class B
  let a: A = A

actor Owner
  new create(main: Main) =>
    var b: B iso = recover B end
    let t: B tag = b
    main.take(consume b, t)

actor Main
  // Main's GC traces the tag field before the iso field.
  var _t: (B tag | None) = None
  var _i: (B iso | None) = None
  // The address of the A inside the B. A reference to the A itself would be
  // traced and keep it in the map.
  var _a: USize = 0

  new create(env: Env) =>
    Owner(this)

  be take(i: B iso, t: B tag) =>
    _a = digestof i.a
    _t = t
    _i = consume i
    @pony_triggergc(@pony_ctx())
    check()

  be check() =>
    // pony_triggergc set the threshold to 0; a GC resets it.
    let gc_ran = @gc_heap_next_gc(this) != 0
    // Main still holds a reference to the A inside its B after its GC.
    let ok = @gc_foreign_has_object(this, _a)
    @pony_exitcode(I32(if gc_ran and ok then 1 else 0 end))
