// main uses lib's private _H, through lib's public alias PubH, as the type
// argument of lib's Foo. Bar overrides show(), but the literal from Bar.mk()
// inherits Foo.show(), which calls x.name() on the _H. It must print
// "lib._H": the call is checked in lib, where _H is visible.
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
  fun show(x: l.PubH): String => "bar"
actor Main
  new create(env: Env) => env.out.print(Bar.mk().show(l.MakeH()))
