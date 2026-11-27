// Foo.g returns a literal that overrides mk() to return U8 and inherits f(),
// which assigns None to v. With C as an inheritor, every copy must compile;
// expects none|3|3.
trait Foo
  fun mk(): (U8 | None) => None
  fun f(): String =>
    var v = mk()
    let s1 = match v | let u: U8 => u.string() | None => "none" end
    v = None
    s1
  fun g(): Foo val => object val is Foo fun mk(): U8 => 3 end
class val C is Foo
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.f() + "|" + C.g().f() + "|" + C.g().g().f())
