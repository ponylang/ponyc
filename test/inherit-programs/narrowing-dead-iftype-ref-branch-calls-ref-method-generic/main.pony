// For G[Bar val] the iftype A <: Bar ref branch cannot run, and it calls a ref
// method on a. That branch must compile anyway; expects other.
class Bar
  var n: U8 = 0
  new create() => None
  fun ref mutate(): U8 => n = n + 1; n
primitive G[A: Bar #read]
  fun f(a: A): String =>
    iftype A <: Bar ref then a.mutate().string() else "other" end
actor Main
  new create(env: Env) => env.out.print(G[Bar val].f(recover val Bar end))
