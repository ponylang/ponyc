// Bar in main is lib's Foo[PubH], a public alias of lib's private _H.
// Foo.g() returns a Foo[Wrap[A]] literal that never calls a method on A.
// Bar.g(l.MakeH()).show() must print "grown".
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.g(l.MakeH()).show())
