// T.f binds a string from an iftype on the class's type argument A, then
// returns a lambda that captures it from an iftype branch. C1 (A = U8) must
// print "u8 first 3" and C2 (A = String) "first t": each class's copy
// keeps the narrowing for its own A.
trait T[A: Stringable val]
  fun f[Z: Any val](z: Z): {(): String} val =>
    let tag' = iftype A <: U8 then "u8 " else "" end
    iftype Z <: Array[A] val then
      {()(z): String =>
        try tag' + "first " + z(0)?.string() else "empty" end} val
    else
      {(): String => "no"} val
    end
class C1 is T[U8]
class C2 is T[String]
actor Main
  new create(env: Env) =>
    env.out.print(C1.f[Array[U8] val]([as U8: 3])())
    env.out.print(C2.f[Array[String] val](["t"])())
