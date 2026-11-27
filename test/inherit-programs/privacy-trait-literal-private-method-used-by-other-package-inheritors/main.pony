// Trait Foo in lib calls a private method of an object literal it creates.
// Classes in main and in p2 inherit go(). The privacy check uses lib, where
// the call is written, so both compile; expects 14 14.
use l = "lib"
use "p2"
class M is l.Foo
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(M.go().string() + " " + P2.go().string())
