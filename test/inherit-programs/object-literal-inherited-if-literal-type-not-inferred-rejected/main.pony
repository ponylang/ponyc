// An object literal's mk returns String, so in its copy of Foo.f the if mixes
// a String with the literal 1. Expects the literal-type error and that x's
// type can't be inferred, each with the note naming the inherited f.
trait Foo
  fun mk(): (U8 | String)
  fun f(c: Bool): String =>
    let x = if c then mk() else 1 end
    x.string()
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): String => "s" end
    env.out.print(o.f(false))
    env.out.print(o.f(true))
