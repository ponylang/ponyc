use "lib:codegen-trace-additional"

use @gc_foreign_has_object[Bool](target: Main, obj: Any tag)
use @gc_heap_next_gc[USize](target: Main)
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])
use @pony_exitcode[None](code: I32)

class A

class B
  let a: A = A
  // The GC traces a val's fields only when its type might reference an
  // actor. This field makes B such a type.
  let actor_field: (Main | None) = None

class C
  let b: B val
  new val create(b': B val) => b = b'

actor Owner
  new create(main: Main) =>
    let b: B val = recover B end
    main.take_c(C(b))
    main.take_t(b)

actor Main
  // The val field comes first, but Main's GC reaches the B through the C
  // only after it has traced the tag field.
  var _c: (C val | None) = None
  var _t: (B tag | None) = None

  new create(env: Env) =>
    Owner(this)

  be take_c(c: C val) =>
    _c = c

  be take_t(t: B tag) =>
    _t = t
    @pony_triggergc(@pony_ctx())
    check()

  be check() =>
    // pony_triggergc set the threshold to 0; a GC resets it.
    let gc_ran = @gc_heap_next_gc(this) != 0
    // Main still holds a reference to the A inside the C's B after its GC.
    let ok =
      match _c
      | let c: C val => @gc_foreign_has_object(this, c.b.a)
      else
        false
      end
    @pony_exitcode(I32(if gc_ran and ok then 1 else 0 end))
