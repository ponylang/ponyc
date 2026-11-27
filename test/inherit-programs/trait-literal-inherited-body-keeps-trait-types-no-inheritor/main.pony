// g() returns a literal that overrides mk() to return U8 and inherits f(),
// which assigns None to v. The literal's f keeps Foo's (U8 | None) for v, so
// the trait compiles with no inheritor; expects ok.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    var v = mk()
    let s1 = match v | let u: U8 => u.string() | None => "none" end
    v = None
    s1
  fun g(): Foo val => object val is Foo fun mk(): U8 => 3 end
actor Main
  new create(env: Env) => env.out.print("ok")
