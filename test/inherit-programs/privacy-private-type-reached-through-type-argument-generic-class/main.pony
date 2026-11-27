// lib's generic class G[A].bar calls a.get().y(). l.G[MyThing] reaches main's
// private _MainPriv through MyThing.get. The call must compile and print
// main._MainPriv.
use l = "lib"
primitive _MainPriv
  fun y(): String => "main._MainPriv"
primitive MyThing
  fun get(): _MainPriv => _MainPriv
actor Main
  new create(env: Env) => env.out.print(l.G[MyThing].bar(MyThing))
