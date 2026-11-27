// Foo, in lib, returns a literal whose show calls name() on A inside a lambda.
// Bar, in main, passes an alias of lib's private _H as A. Expects the private
// lookup error at the lambda's call, with notes for Bar's inherited g and the
// type argument: a lambda nested in the literal is judged the same way.
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.g(l.MakeH()).show())
