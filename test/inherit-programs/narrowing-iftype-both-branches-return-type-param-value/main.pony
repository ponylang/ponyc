// Foo.id returns x from both branches of iftype A <: Stringable. Bar is
// Foo[Array[U8] val], which is not Stringable; id must return the array, and
// its size, 1, is printed.
trait Foo[A: Any val]
  fun id(x: A): A =>
    iftype A <: Stringable val then x else x end
class Bar is Foo[Array[U8] val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.id([as U8: 1]).size().string())
