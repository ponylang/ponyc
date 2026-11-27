// Foo, in lib, returns a literal providing Look[A] that calls a method on A.
// Bar, in main, passes an alias of lib's private _H as A. Expects the private
// lookup error with notes for Bar's inherited g and the type argument: main
// may not call methods on lib's private type, even through lib's code.
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.g().look(l.MakeH()))
