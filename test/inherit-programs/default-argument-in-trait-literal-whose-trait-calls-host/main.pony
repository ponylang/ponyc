// T.mk creates an object literal that is S, with a method default Three().
// S's other() calls C.mk() back. o.g() must use the default; expects 3s.
primitive Three
  fun apply(): U8 => 3
trait T
  fun label(): String => "t"
  fun mk(): String =>
    let o = object is S fun g(x: U8 = Three()): U8 => x end
    o.g().string() + o.label()
trait S
  fun label(): String => "s"
  fun other(): String => C.mk()
class C is T
actor Main
  new create(env: Env) => env.out.print(C.mk())
