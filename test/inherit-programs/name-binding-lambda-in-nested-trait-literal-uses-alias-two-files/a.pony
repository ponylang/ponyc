// In b.pony, T.m's object literal holds a lambda that uses c.List, where c is
// b.pony's alias for collections; a.pony has no such alias. Calling through
// two levels of literals must resolve c in b.pony and print n1.
primitive K
  fun mk(): T[U8] val => object val is T[U8] end
actor Main
  new create(env: Env) => env.out.print(K.mk().m().m().n())
