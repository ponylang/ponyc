// For Bar the iftype branch that calls mk() cannot run, and Bar's mk() returns
// String. x must still compile as a join with None; expects none.
trait Foo[A: Any val]
  fun mk(): (A | None)
  fun f(): String =>
    let x = iftype A <: U8 then mk() else None end
    match x | None => "none" else "some" end
class Bar is Foo[String]
  new create() => None
  fun mk(): String => "s"
actor Main
  new create(env: Env) => env.out.print(Bar.f())
