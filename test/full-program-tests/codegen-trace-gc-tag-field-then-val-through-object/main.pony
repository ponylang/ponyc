use "lib:codegen-trace-additional"

use @gc_foreign_has_object[Bool](target: Main, obj: Any tag)
use @gc_heap_next_gc[USize](target: Main)
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])
use @pony_exitcode[None](code: I32)

class A

class B
  let a: A = A
  // A val's fields are traced only if its type can reference an actor. This
  // field lets B reference one.
  let actor_field: (Main | None) = None

class C
  let b: B val
  // This field lets C reference an actor, so a C val's fields are traced.
  let actor_field: (Main | None) = None
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
    // pony_triggergc sets the heap's next-GC threshold to 0, and the GC that
    // runs after the previous behaviour raises it.
    let gc_ran = @gc_heap_next_gc(this) != 0
    let a_still_referenced =
      match _c
      | let c: C val => @gc_foreign_has_object(this, c.b.a)
      else
        false
      end
    @pony_exitcode(I32(if gc_ran and a_still_referenced then 1 else 0 end))
