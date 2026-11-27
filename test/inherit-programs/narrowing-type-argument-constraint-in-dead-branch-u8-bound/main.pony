// G[U8] provides T[Array[U8] val], and m()'s iftype A <: U8 branch names
// G[A], which needs A to be a U8. In G's copy A is an Array, so the branch
// can't run. Expects "other": G[A] outside its constraint there is no error.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): String => iftype A <: U8 then G[A].n() else "other" end
class val G[A: U8] is T[Array[A] val]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(G[U8].m())
