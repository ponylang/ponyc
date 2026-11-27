// In an object literal's copy of f, mk() is (B2 | None), and string() is found
// only through the iftype on A, which may not hold for the literal. Expects
// that error with the hint to bind a local first. The method returns a ref.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then mk().string() else "other" end
primitive Mk
  fun make[B2: Any val](): Foo[(B2 | None)] =>
    object is Foo[(B2 | None)] fun mk(): (B2 | None) => None end
actor Main
  new create(env: Env) =>
    env.out.print(Mk.make[U8]().f())
    env.out.print(Mk.make[Array[U8] val]().f())
