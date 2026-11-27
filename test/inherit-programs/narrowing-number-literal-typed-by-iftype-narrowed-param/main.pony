// Inside iftype A <: U8 val, the literal 3 is typed A. K.g[String] takes the
// else branch and prints o.
primitive K
  fun g[A: Any val](): String =>
    iftype A <: U8 val then let y: A = 3; y.string() else "o" end
actor Main
  new create(env: Env) => env.out.print(K.g[String]())
