// A generic class's method calls itself on A[Array[T] val], so each call needs
// a larger type, with no trait or literal involved. The test expects the
// reachability depth-limit error that a growing trait literal is compared to.
class A[T: Any val]
  new create() => None
  fun f(n: USize): USize => if n == 0 then 0 else A[Array[T] val].f(n - 1) + 1 end
actor Main
  new create(env: Env) => env.out.print(A[U8].f(3).string())
