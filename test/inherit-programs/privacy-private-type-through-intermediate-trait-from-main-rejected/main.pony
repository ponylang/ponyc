// main fills lib's Mid with l.PubH, and Mid fills Foo with Box[X], so the
// inherited f() calls name() on lib's private _H. Expects the private-type
// lookup error with a note pointing at main's type argument.
use l = "lib"
class Bar is l.Mid[l.PubH]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f(l.Box[l.PubH](l.MakeH())))
