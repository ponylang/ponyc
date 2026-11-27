// lib's FooH fills Foo with lib's private _H, and f() narrows A with iftype
// before calling name(). Expects "lib._H": the narrowed call is written in
// lib, so calling a method on lib's private type is allowed.
use l = "lib"
class Bar is l.FooH
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f(l.MakeH()))
