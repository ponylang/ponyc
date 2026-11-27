// lib's MidH fills Mid with _H, and Mid fills Foo with Box[X], so the
// inherited f() calls name() on lib's private _H. Expects "lib._H": every type
// argument on the way is written in lib, so main's Bar may inherit f.
use l = "lib"
class Bar is l.MidH
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f(l.Box[l.PubH](l.MakeH())))
