// For Bar, the iftype branch that breaks out of the loop with a U32 cannot run.
// Bar's copy must compile and the loop must end normally; expects y.
trait Foo[A: Any val]
  fun f(): String =>
    var i: USize = 0
    let r = while i < 3 do
      i = i + 1
      iftype A <: U8 then break U32(7) else "x" end
      "y"
    else
      "z"
    end
    r.string()
class Bar is Foo[String]
actor Main
  new create(env: Env) => env.out.print(Bar.f())
