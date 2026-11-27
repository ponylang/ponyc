// For Bar, the iftype branch that creates an object literal with a field
// cannot run, and f() reads that field after the iftype. Bar's copy must
// compile; expects early.
trait Foo[A: Any val]
  fun f(): String =>
    let o = iftype A <: U8 then
      object
        let v: U8 = 3
        fun name(): String => "anon"
      end
    else
      return "early"
    end
    o.v.string() + o.name()
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f())
