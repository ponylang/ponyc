// Foo.f's iftype A <: U8 branch falls through and its else returns. Bar has
// A = String, so its copy never runs the first branch, but the code after the
// iftype must not be reported as unreachable. Bar.f() must print "other".
trait Foo[A: Any val]
  fun f(): String =>
    iftype A <: U8 then
      None
    else
      return "other"
    end
    "u8"
class Bar is Foo[String]
actor Main
  new create(env: Env) => env.out.print(Bar.f())
