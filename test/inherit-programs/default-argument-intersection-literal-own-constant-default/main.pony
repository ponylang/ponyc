// T.mk makes a literal of (T & S) whose g defaults to 3. Calling o.g() in
// T.mk must print 3o.
trait T
  fun label(): String => "t"
  fun mk(): String =>
    let o = object is (T & S)
      fun label(): String => "o"
      fun g(x: U8 = 3): U8 => x
    end
    o.g().string() + o.label()
trait S
  fun label(): String => "s"
class C is T
actor Main
  new create(env: Env) => env.out.print(C.mk())
