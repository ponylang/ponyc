// main's Bar fills lib's Foo with l.PubH, a public alias of lib's private _H,
// and the inherited f() calls name() on it. Expects the private-type lookup
// error with a note pointing at main's type argument, because main may not use
// lib's private type.
use l = "lib"
class Bar is l.Foo[l.PubH]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f(l.MakeH()))
