// Like the plain case, but the literal also has extra(), which only it has
// and which f() calls after the iftype. Bar's copy must compile; expects early.
trait Named
  fun name(): String
trait Foo[A: Any val]
  fun f(): String =>
    let o = iftype A <: U8 then
      object is Named
        fun name(): String => "anon"
        fun extra(): String => "extra"
      end
    else
      return "early"
    end
    o.name() + o.extra()
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f())
