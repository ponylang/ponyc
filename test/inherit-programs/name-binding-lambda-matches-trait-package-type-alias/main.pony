// lib's Foo.make has a lambda that matches on Num, a type alias in lib. Bar
// in main and Inside in lib inherit make; both must resolve Num in lib and
// print u8.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())
    env.out.print(l.Inside.make())

class Bar is l.Foo
