// T.f infers a local's type from get(), which returns (U64 | None), and then
// assigns None to it. No type provides T; the program must compile and print
// "x".
trait T
  fun get(): (U64 | None)
  fun f(): String =>
    var x = get()
    x = None
    "ok"
actor Main
  new create(env: Env) => env.out.print("x")
