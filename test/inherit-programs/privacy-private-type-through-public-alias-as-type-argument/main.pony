// main passes lib's private primitive _H to lib's Box only through lib's
// public alias PubH. Box's show calls name() on it. Naming a private type
// through a public alias from another package is allowed, so the program must
// compile and print "lib._H".
use l = "lib"
actor Main
  new create(env: Env) =>
    env.out.print(l.Box[l.PubH](l.MakeH()).show())
