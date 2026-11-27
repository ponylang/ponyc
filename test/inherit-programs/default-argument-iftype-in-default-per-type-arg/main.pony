// A default argument holds an iftype on the class type parameter.
// Bar[String] must get other and Bar[U8] must get 255.
class Bar[A: Any val]
  new create() => None
  fun f(s: String = iftype A <: U8 then A.max_value().string() else "other" end): String => s
actor Main
  new create(env: Env) => env.out.print(Bar[String].f()); env.out.print(Bar[U8].f())
