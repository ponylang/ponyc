// lib's public alias PubW is Wrapper[_H], with _H private to lib, and Bar
// provides Foo[l.PubW]. The test expects "lib._H": _H inside the alias body
// is judged in lib, where the alias is written.
use l = "lib"
class Bar is l.Foo[l.PubW]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f(l.MakeW()))
