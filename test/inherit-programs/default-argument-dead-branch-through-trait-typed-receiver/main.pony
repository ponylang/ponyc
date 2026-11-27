// f's default argument has an iftype branch that calls A.max_value(), which
// only a U8 A has. Main calls f through a Foo[String], so the default is copied
// into the call with A as String and that branch can't run. The program must
// compile and print "other".
trait Foo[A: Any val]
  fun f(s: String = iftype A <: U8 then A.max_value().string() else "other" end): String => s
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) =>
    let t: Foo[String] = Bar
    env.out.print(t.f())
