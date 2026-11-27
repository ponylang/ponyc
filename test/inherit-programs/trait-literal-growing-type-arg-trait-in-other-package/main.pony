// lib's T.m returns an object literal of T[Array[A] val], so each call adds a
// level of Array. Starting from an object literal of l.T[U8] in main and
// calling m three times must compile and print n.
use l = "lib"
actor Main
  new create(env: Env) =>
    let t: l.T[U8] val = object val is l.T[U8] end
    env.out.print(t.m().m().m().n())
