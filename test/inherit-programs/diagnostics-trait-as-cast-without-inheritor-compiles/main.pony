// A trait method casts (U8 | None) to U8 with as, and nothing inherits it.
// The program must compile and print nothing.
trait Foo
  fun mk(): (U8 | None)
  fun f(): U8 => try mk() as U8 else 0 end
actor Main
  new create(env: Env) => None
