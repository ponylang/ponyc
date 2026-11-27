// lib declares `use u = "../ua"`, and main uses `u` and a class Bar that
// nothing declares. The test expects "can't find declaration" for both: a
// `use` alias belongs to the package that wrote it.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())
    env.out.print(u.Thing.name() + " (leaked alias)")
