// main's val class Bar fills lib's Foo with l.PubH, a public alias of lib's
// private _H, and the inherited show() calls name() on it. Expects the
// private-type lookup error with a note pointing at main's type argument.
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.show(l.MakeH()))
