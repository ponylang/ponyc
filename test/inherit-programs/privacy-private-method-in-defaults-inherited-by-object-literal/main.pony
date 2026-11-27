// An object literal in main provides lib's Foo, whose default d calls lib's
// private method _secret. The call is judged in lib, where it is written, so
// the program must print "secret".
use l = "lib"
actor Main
  new create(env: Env) =>
    let o = object is l.Foo end
    env.out.print(o.d())
