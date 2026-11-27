// Foo, in lib, returns a literal that captures x: A and calls x.name(). Bar,
// in main, passes an alias of lib's private _H as A. Expects the private
// lookup error at that call, with notes for Bar's inherited g and the type
// argument: main may not call methods on lib's private type through lib's code.
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.g(l.MakeH()).show())
