// main passes lib's private _H, through lib's public alias PubH, as the type
// argument of lib's Foo. Bar's copy of g() makes a literal that calls
// x.name() on the _H. main wrote that type argument, so the program must fail
// with the private-type error, naming main's PubH as where _H came from.
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.k().g(l.MakeH()).show())
