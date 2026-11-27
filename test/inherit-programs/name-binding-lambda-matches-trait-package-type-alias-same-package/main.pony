// lib's Foo.make has a lambda that matches on Num, a type alias in lib. Only
// Inside, also in lib, inherits make. The program must print u8.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(l.Inside.make())
