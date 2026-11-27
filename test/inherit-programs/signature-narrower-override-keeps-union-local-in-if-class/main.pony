// Foo's default assigns `if c then mk() else 1 end` to a (U8 | String) local,
// and C overrides mk() to return String. The test expects 1: the literal 1
// still takes U8 from the local's declared type.
trait Foo
  fun mk(): (U8 | String)
  fun f(c: Bool): String =>
    let x: (U8 | String) = if c then mk() else 1 end
    x.string()
class C is Foo
  new create() => None
  fun mk(): String => "s"
actor Main
  new create(env: Env) => env.out.print(C.f(false))
