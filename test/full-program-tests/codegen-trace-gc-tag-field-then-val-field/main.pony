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

actor Owner
  new create(main: Main) =>
    let b: B val = recover B end
    main.take(b, b)

actor Main
  // With a tag field on each side of the val field, Main's GC traces a tag
  // first whichever order it traces fields in.
  var _t: (B tag | None) = None
  var _v: (B val | None) = None
  var _t2: (B tag | None) = None

  new create(env: Env) =>
    Owner(this)

  be take(v: B val, t: B tag) =>
    _t = t
    _v = v
    _t2 = t
    @pony_triggergc(@pony_ctx())
    check()

  be check() =>
    // pony_triggergc sets the heap's next-GC threshold to 0, and the GC that
    // runs after the previous behaviour raises it.
    let gc_ran = @gc_heap_next_gc(this) != 0
    let a_still_referenced =
      match _v
      | let v: B val => @gc_foreign_has_object(this, v.a)
      else
        false
      end
    @pony_exitcode(I32(if gc_ran and a_still_referenced then 1 else 0 end))
