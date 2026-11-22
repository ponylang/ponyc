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
  // With a tag field on each side of the iso field, Main's GC traces a tag
  // first whichever order it traces fields in.
  var _t: (B tag | None) = None
  var _i: (B iso | None) = None
  var _t2: (B tag | None) = None
  // The address of the A inside the B. A reference to the A itself would be
  // traced and keep it in Main's foreign map.
  var _a: USize = 0

  new create(env: Env) =>
    Owner(this)

  be take(i: B iso, t: B tag) =>
    _a = digestof i.a
    _t = t
    _i = consume i
    _t2 = t
    @pony_triggergc(@pony_ctx())
    check()

  be check() =>
    // pony_triggergc sets the heap's next-GC threshold to 0, and the GC that
    // runs after the previous behaviour raises it.
    let gc_ran = @gc_heap_next_gc(this) != 0
    let a_still_referenced = @gc_foreign_has_object(this, _a)
    @pony_exitcode(I32(if gc_ran and a_still_referenced then 1 else 0 end))
