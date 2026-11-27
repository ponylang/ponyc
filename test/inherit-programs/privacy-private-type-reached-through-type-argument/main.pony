// lib's T[A].bar calls a.get().y(). C is l.T[MyThing], and MyThing.get returns
// main's private _MainPriv. C's bar must call y on it and print
// main._MainPriv.
use l = "lib"
primitive _MainPriv
  fun y(): String => "main._MainPriv"
primitive MyThing
  fun get(): _MainPriv => _MainPriv
class C is l.T[MyThing]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar(MyThing))
