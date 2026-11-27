// T.mk makes a literal whose g defaults to Three(). T is provided only by
// an object literal in Main; calling mk on it must print 3t.
primitive Three
  fun apply(): U8 => 3
trait T
  fun label(): String => "t"
  fun mk(): String =>
    let o = object is T fun g(x: U8 = Three()): U8 => x end
    o.g().string() + o.label()
actor Main
  new create(env: Env) =>
    let o: T val = object val is T end
    env.out.print(o.mk())
