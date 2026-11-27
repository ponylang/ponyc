// T's foo() returns Stringable, and C overrides it to return l.PubH, an alias
// of lib's private _H, so bar() calls string() on _H in C's copy. Expects the
// private-type lookup error with a note pointing at C's foo() result type.
use l = "lib"
class C is l.T
  new create() => None
  fun foo(): l.PubH => l.MakeH()
actor Main
  new create(env: Env) => env.out.print(C.bar())
