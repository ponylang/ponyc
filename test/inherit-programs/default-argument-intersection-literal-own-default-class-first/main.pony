// T.mk makes a literal of (T & S) whose g defaults to Three(), with C and
// Main declared before the traits. Calling o.g() in T.mk must print 3o.
primitive Three
  fun apply(): U8 => 3
class C is T
actor Main
  new create(env: Env) => env.out.print(C.mk())
trait S
  fun label(): String => "s"
trait T
  fun label(): String => "t"
  fun mk(): String =>
    let o = object is (T & S)
      fun label(): String => "o"
      fun g(x: U8 = Three()): U8 => x
    end
    o.g().string() + o.label()
