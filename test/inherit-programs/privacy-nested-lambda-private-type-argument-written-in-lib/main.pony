// lib's FooH fills Foo with lib's private _H, and f() calls name() inside a
// lambda nested in another lambda. Expects "lib._H": both lambdas are written
// in lib, so the call on the private type is allowed.
use l = "lib"
class Bar is l.FooH
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f(l.MakeH()))
