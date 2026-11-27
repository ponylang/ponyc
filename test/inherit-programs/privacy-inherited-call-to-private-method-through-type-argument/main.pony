// lib's Foo[A].f calls the private method _secret on A, constrained by lib's
// HasSecret. main's Bar provides Foo[M], where main's M implements _secret.
// The call is judged in lib, where it is written, so the program must print
// "main secret".
use l = "lib"
primitive M is l.HasSecret
  fun _secret(): String => "main secret"
class Bar is l.Foo[M]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f(M))
