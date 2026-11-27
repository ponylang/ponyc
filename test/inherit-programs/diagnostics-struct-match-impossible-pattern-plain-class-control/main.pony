// A plain class matches a struct value against String ref. The pattern can
// never match, so the program must fail with that error.
struct S
  new create() => None
class C
  new create() => None
  fun get(): S => S
  fun f(): String =>
    match get()
    | let s: String ref => "s"
    else "o" end
actor Main
  new create(env: Env) => env.out.print(C.f())
