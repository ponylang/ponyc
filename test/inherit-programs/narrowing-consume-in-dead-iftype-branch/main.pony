// For Bar, the iftype branch that consumes b cannot run. Bar's copy must
// compile; expects ok.
class Box1
  new iso create() => None
trait Foo[A: Any val]
  fun f(): String =>
    let b: Box1 iso = Box1
    iftype A <: U8 then let c: Box1 iso = consume b else None end
    "ok"
class Bar is Foo[String]
actor Main
  new create(env: Env) => env.out.print(Bar.f())
