// C's get() returns a String, so in C's copy of f() the Main tag case cannot
// match. It must not be reported as an error; expects o.
trait T
  fun get(): Any tag
  fun f(): String =>
    match get()
    | let s: Main tag => "main"
    else "o" end
class C is T
  new create() => None
  fun get(): (String val | String ref) => String
actor Main
  new create(env: Env) => env.out.print(C.f())
