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

actor Owner
  new create(main: Main) =>
    let b: B val = recover B end
    main.take(b, b)

actor Main
  // Main's GC traces the tag field before the val field.
  var _t: (B tag | None) = None
  var _v: (B val | None) = None

  new create(env: Env) =>
    Owner(this)

  be take(v: B val, t: B tag) =>
    _t = t
    _v = v
    @pony_triggergc(@pony_ctx())
    check()

  be check() =>
    // pony_triggergc set the threshold to 0; a GC resets it.
    let gc_ran = @gc_heap_next_gc(this) != 0
    // Main still holds a reference to the A inside its B after its GC.
    let ok =
      match _v
      | let v: B val => @gc_foreign_has_object(this, v.a)
      else
        false
      end
    @pony_exitcode(I32(if gc_ran and ok then 1 else 0 end))
