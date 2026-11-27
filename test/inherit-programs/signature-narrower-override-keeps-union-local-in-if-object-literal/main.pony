// Foo's default assigns `if c then mk() else 1 end` to a (U8 | String) local,
// and an object literal overrides mk() to return String. The test expects 1
// and s, one line for each branch.
trait Foo
  fun mk(): (U8 | String)
  fun f(c: Bool): String =>
    let x: (U8 | String) = if c then mk() else 1 end
    x.string()
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): String => "s" end
    env.out.print(o.f(false))
    env.out.print(o.f(true))
