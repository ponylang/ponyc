// T.mk makes a literal that provides S, a subtrait of T, and has g with the
// default Three(). Calling o.g() must print 3, and label comes from S: 3s.
primitive Three
  fun apply(): U8 => 3
trait T
  fun label(): String => "t"
  fun mk(): String =>
    let o = object is S fun g(x: U8 = Three()): U8 => x end
    o.g().string() + o.label()
trait S is T
  fun label(): String => "s"
class C is T
actor Main
  new create(env: Env) => env.out.print(C.mk())
