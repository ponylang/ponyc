// Inside the iftype on A, f binds mk() to a Stringable val local before
// calling string(). An object literal of Foo[(B2 | None)] expects "None" for
// U8 and "other" for Array[U8] val.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then
      let v: Stringable val = mk()
      v.string()
    else "other" end
primitive Mk
  fun make[B2: Any val](): Foo[(B2 | None)] =>
    object ref is Foo[(B2 | None)] fun mk(): (B2 | None) => None end
actor Main
  new create(env: Env) =>
    env.out.print(Mk.make[U8]().f())
    env.out.print(Mk.make[Array[U8] val]().f())
