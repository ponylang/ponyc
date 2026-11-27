// For G[String val] the iftype A <: Bar ref branch cannot run, and it calls
// Bar's ref method on a. That branch must compile anyway; expects other.
class Bar

  fun ref mutate(): U8 => 1
primitive G[A: Any #read]
  fun f(a: A): String =>
    iftype A <: Bar ref then a.mutate().string() else "other" end


actor Main
  new create(env: Env) => env.out.print(G[String val].f("x"))
