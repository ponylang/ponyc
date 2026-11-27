// Foo, in lib, returns a literal of Foo[A] with no captures whose look calls a
// method on A. Bar, in main, passes an alias of lib's private _H as A. Expects
// the private lookup error at that call, with notes for Bar's inherited g and
// the type argument.
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.g().look(l.MakeH()))
