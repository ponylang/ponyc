// lib's FooBH fills Foo with Box[_H], so the inherited f() calls name() on
// lib's private _H. Expects "lib._H": the type argument is written in lib, so
// main's Bar may inherit f.
use l = "lib"
class Bar is l.FooBH
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f(l.Box[l.PubH](l.MakeH())))
