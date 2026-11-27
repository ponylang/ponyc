// A lib trait default binds `me = this` and captures `me` in a lambda, which
// the compiler rejects with "T box is not a subtype of T ref^". No type
// provides the trait. The test expects that one error from the trait's text.
use l = "lib"
actor Main
  new create(env: Env) => env.out.print(l.L.x())
