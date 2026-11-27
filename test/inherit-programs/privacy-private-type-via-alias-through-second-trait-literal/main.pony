// main uses lib's private _H, through lib's public alias PubH, as the type
// argument of lib's Foo. Foo.g() returns a Foo2 literal whose h() builds a
// literal that calls x.name() on the _H. The program must print "lib._H":
// the call is checked in lib, where _H is visible.
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.g().h(l.MakeH()).show())
