// The inherited bar() calls x() on the private _X that Lib.make() returns.
// Expects "lib._X": the body is written in lib, so calling a method on lib's
// private type is allowed even though main's class C inherits it.
use l = "lib"
class C is l.T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar())
