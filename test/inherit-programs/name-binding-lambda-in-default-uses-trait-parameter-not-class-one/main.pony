// Bar[In: U8] provides Foo[String], and Foo's default has a lambda that checks
// `iftype In <: String`. The test expects "In=String": In in the lambda is
// Foo's parameter, bound to String, not Bar's parameter of the same name.
trait Foo[In: Any val]
  fun describe(x: In): String =>
    let f = {(y: In): String => iftype In <: String then "In=String" else "In=other" end}
    f(x)

class Bar[In: U8] is Foo[String]
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(Bar[U8].describe("s"))
