// C provides T and implements get(): Any ref by returning a struct. A struct
// is not a subtype of an interface, so C does not implement T and the
// program must fail with "type does not implement its provides list".
struct S
  new create() => None
trait T
  fun get(): Any ref
  fun f(): String =>
    match get()
    | let s: String ref => "s"
    else "o" end
class C is T
  new create() => None
  fun get(): S => S
actor Main
  new create(env: Env) => env.out.print(C.f())
