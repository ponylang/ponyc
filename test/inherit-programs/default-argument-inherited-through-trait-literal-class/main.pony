// T.k returns an object literal that is a T. Calling f() on it, and on a
// literal made by it, must use T's default 3; expects 33.
trait T
  fun f(x: U8 = 3): String => x.string()
  fun k(): T val => object val is T end
class val C is T
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.k().f() + C.k().k().f())
