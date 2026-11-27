// lib's generic trait Foo has an f() that calls name() on lib's private _H
// directly, and main's Bar fills Foo with a public alias of _H. Expects
// "lib._H": the body is written in lib, so it may name lib's private type.
use l = "lib"
class Bar is l.Foo[l.PubH]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f(l.MakeH()))
