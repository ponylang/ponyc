// An object literal in Main overrides mk() without ? and inherits f(), which
// calls mk()? in a try. The copy must compile; expects 3.
trait Foo
  fun mk(): U8 ?
  fun f(): U8 => try mk()? else 0 end
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): U8 => 3 end
    env.out.print(o.f().string())
