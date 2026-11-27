// C overrides foo() to return l.PubL, an alias of lib's private _L written in
// main, and the inherited bar() calls x() on foo(). Expects the private-type
// lookup error with a note pointing at C's foo() result type, because main may
// not call methods on lib's private type.
use l = "lib"
class C is l.T
  new create() => None
  fun foo(): l.PubL => l.MakeL()
actor Main
  new create(env: Env) => env.out.print(C.bar())
