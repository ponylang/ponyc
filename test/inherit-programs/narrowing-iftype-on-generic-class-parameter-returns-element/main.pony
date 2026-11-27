// Q[B2] provides Foo[Array[U8] val, B2], so f()'s iftype A <: Array[B] branch
// runs only when B2 is U8. Expects "5" for Q[U8] and "none" for Q[String]: the
// branch is chosen per instantiation of Q.
trait Foo[A: Any val, B: Any val]
  fun f(x: A): (B | None) =>
    iftype A <: Array[B] val then
      try x(0)? else None end
    else None end
class Q[B2: Any val] is Foo[Array[U8] val, B2]
  new create() => None
actor Main
  new create(env: Env) =>
    match Q[U8].f([as U8: 5]) | let u: U8 => env.out.print(u.string()) else env.out.print("none") end
    match Q[String].f([as U8: 5]) | let s: String => env.out.print(s) else env.out.print("none") end
