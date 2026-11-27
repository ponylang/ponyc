// For Bar, the iftype branch that creates an object literal cannot run, and
// the other branch returns early. Bar's copy must compile; expects early.
trait Named
  fun name(): String
trait Foo[A: Any val]
  fun f(): String =>
    let o = iftype A <: U8 then
      object is Named fun name(): String => "anon" end
    else
      return "early"
    end
    o.name()
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f())
