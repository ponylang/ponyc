// In an object literal's copy of f, mk() is (B2 | None), and string() is found
// only through the iftype on A, which may not hold for the literal. Expects
// that error with the hint to bind a local first. The literal is a local.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then mk().string() else "other" end
primitive Mk
  fun make2[B2: Any val](): Foo[(B2 | None)] ref =>
    let o = object ref is Foo[(B2 | None)] fun mk(): (B2 | None) => None end
    o
actor Main
  new create(env: Env) =>
    env.out.print(Mk.make2[U8]().f())
    env.out.print(Mk.make2[Array[U8] val]().f())
