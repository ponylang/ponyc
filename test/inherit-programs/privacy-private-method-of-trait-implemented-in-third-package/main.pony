// lib's trait HasSecret declares the private method _secret(), p3's M
// implements it, and lib's inherited f() calls it. Expects "p3 secret" twice:
// the call is written in lib, the package that declares _secret(), so it is
// allowed for both a generic function and an inherited method.
use l = "lib"
use p3 = "p3"
class Bar is l.Foo[p3.M]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(l.G.g[p3.M](p3.M))
    env.out.print(Bar.f(p3.M))
