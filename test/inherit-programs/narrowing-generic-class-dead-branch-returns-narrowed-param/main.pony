// For Bar[String] the iftype A <: U8 branch cannot run, and it returns x as a
// U8. A generic class must compile this; expects 7.
class Bar[A: Any val]
  new create() => None
  fun f(x: A): U8 =>
    iftype A <: U8 then x else U8(7) end
actor Main
  new create(env: Env) => env.out.print(Bar[String].f("s").string())
