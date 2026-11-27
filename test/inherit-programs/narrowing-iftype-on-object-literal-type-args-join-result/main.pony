// Q[B2].make returns an object literal of Foo[Array[U8] val, B2]. The
// inherited f takes the then branch only when Array[U8] val <: Array[B2] val:
// Q[U8] prints 5 and Q[String] prints none.
trait Foo[A: Any val, B: Any val]
  fun f(x: A): (B | None) =>
    iftype A <: Array[B] val then
      try x(0)? else None end
    else None end
class Q[B2: Any val]
  new create() => None
  fun make(): Foo[Array[U8] val, B2] val => object is Foo[Array[U8] val, B2] end
actor Main
  new create(env: Env) =>
    match Q[U8].make().f([as U8: 5]) | let u: U8 => env.out.print(u.string()) else env.out.print("none") end
    match Q[String].make().f([as U8: 5]) | let s: String => env.out.print(s) else env.out.print("none") end
