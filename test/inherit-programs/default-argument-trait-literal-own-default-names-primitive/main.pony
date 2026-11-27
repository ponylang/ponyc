// T.mk makes a literal that provides T and has g with the default Three().
// Calling o.g() in T.mk must resolve Three and print 3t.
primitive Three
  fun apply(): U8 => 3
trait T
  fun label(): String => "t"
  fun mk(): String =>
    let o = object is T fun g(x: U8 = Three()): U8 => x end
    o.g().string() + o.label()
class C is T
actor Main
  new create(env: Env) => env.out.print(C.mk())
