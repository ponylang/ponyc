// An object literal in main provides lib's Foo, whose default methods use
// lib's private primitive _H by name and as a result type. The defaults are
// judged in lib, so the program must print "lib helper" twice.
use l = "lib"
actor Main
  new create(env: Env) =>
    let o = object is l.Foo end
    env.out.print(o.describe())
    env.out.print(o.d2())
