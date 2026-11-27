// Bar inherits f from lib's trait FooW, which fills Foo's type parameter with
// Wrapper[_H], so f calls name() on lib's private _H. Expects "lib._H": the
// private type is judged in lib, where FooW was written, not in main.
use l = "lib"
class Bar is l.FooW
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f(l.MakeW()))
