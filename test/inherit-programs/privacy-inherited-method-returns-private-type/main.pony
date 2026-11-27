// lib's trait has foo() returning lib's private _L, and bar() calls x() on
// that result. main's class C inherits both. Expects "lib._L": the inherited
// signature and body are judged in lib, where they are written.
use l = "lib"
class C is l.T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar())
