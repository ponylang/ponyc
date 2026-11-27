// C provides T[S] where S is a struct. Structs can't be type arguments, so
// the program must fail with that error at the provides list.
struct S
  new create() => None
trait T[A: Any ref]
  fun get(): A
  fun f(): String =>
    match get()
    | let s: String ref => "s"
    else "o" end
class C is T[S]
  new create() => None
  fun get(): S => S
actor Main
  new create(env: Env) => env.out.print(C.f())
