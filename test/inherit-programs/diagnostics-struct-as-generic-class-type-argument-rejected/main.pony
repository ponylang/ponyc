// Main passes a struct as the type argument of a generic class. Structs can't
// be type arguments, so the program must fail with that error.
struct S
  new create() => None
class G[A: Any ref]
  new create() => None
  fun f(x: A): String =>
    match x
    | let s: String ref => "s"
    else "o" end
actor Main
  new create(env: Env) => env.out.print(G[S].f(S))
