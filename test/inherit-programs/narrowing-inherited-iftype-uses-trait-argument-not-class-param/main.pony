// Bar[In: U8] is Foo[String]: Bar's own In and Foo's In share a name. The
// iftype in the inherited describe must test Foo's argument, String, so Bar
// prints In=String; Baz is Foo[U8] and prints In=other.
trait Foo[In: Any val]
  fun describe(x: In): String =>
    iftype In <: String then "In=String" else "In=other" end

class Bar[In: U8] is Foo[String]
  new create() => None
class Baz is Foo[U8]
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(Bar[U8].describe("s"))
    env.out.print(Baz.describe(3))
