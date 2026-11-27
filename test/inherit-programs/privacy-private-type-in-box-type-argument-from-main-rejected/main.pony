// main fills lib's Foo with l.Box[l.PubH], where PubH is a public alias of
// lib's private _H, so the inherited f() calls name() on _H. Expects the
// private-type lookup error with a note pointing at main's type argument.
use l = "lib"
class Bar is l.Foo[l.Box[l.PubH]]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f(l.Box[l.PubH](l.MakeH())))
