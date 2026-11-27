// A lib trait declares a constructor with a body, which traits can't have. The
// test expects that error and the failed load of the lib module, even though a
// class in main provides the trait.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())

class Bar is l.Foo
