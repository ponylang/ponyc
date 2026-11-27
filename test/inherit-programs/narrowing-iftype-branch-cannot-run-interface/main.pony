// Interface Foo is filled with (P | None) by Baz, so f()'s iftype branch
// can't run in Baz's copy, where mk() returns a P with no string(). Expects
// "other": the call in that branch is not an error.
primitive P
interface Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then mk().string() else "other" end
class Baz is Foo[(P | None)]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.f())
