// main fills lib's Foo with l.PubH, a public alias of lib's private _H, and
// f() narrows A with iftype before calling name(). Expects the private-type
// lookup error with a note pointing at main's type argument: narrowing does
// not hide that main wrote the private type.
use l = "lib"
class Bar is l.Foo[l.PubH]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f(l.MakeH()))
