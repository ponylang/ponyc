// f()'s default argument uses iftype A <: U8 to call A.max_value(), and Bar
// fills A with String, so that branch can't run. Expects "other": String
// having no max_value() is not an error in the default argument's branch.
trait Foo[A: Any val]
  fun f(s: String = iftype A <: U8 then A.max_value().string() else "other" end): String => s
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f())
