// Foo.f tests (A, B) <: (U8, String). Bar is Foo[U8, String] and prints both;
// Baz is Foo[U8, U8] and prints no.
trait Foo[A: Any val, B: Any val]
  fun f(): String =>
    iftype (A, B) <: (U8, String) then "both" else "no" end
class Bar is Foo[U8, String]
class Baz is Foo[U8, U8]
actor Main
  new create(env: Env) => env.out.print(Bar.f()); env.out.print(Baz.f())
