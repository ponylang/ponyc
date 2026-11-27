// T.f returns a lambda from an iftype branch that tests the method's type
// argument against Array[A], where A is the class's type argument. C1 and C3
// have A = U8 and C2 has A = String. Each call must take the branch its own
// types select and print that array's first element, or "no".
trait T[A: Stringable val]
  fun f[Z: Any val](z: Z): {(): String} val =>
    iftype Z <: Array[A] val then
      {()(z): String =>
        try "first " + z(0)?.string() else "empty" end} val
    else
      {(): String => "no"} val
    end

class C1 is T[U8]
class C2 is T[String]
class C3 is T[U8]

actor Main
  new create(env: Env) =>
    env.out.print(C1.f[Array[U8] val]([as U8: 3])())
    env.out.print(C1.f[String]("s")())
    env.out.print(C2.f[Array[String] val](["t"])())
    env.out.print(C3.f[Array[U8] val]([as U8: 9])())
