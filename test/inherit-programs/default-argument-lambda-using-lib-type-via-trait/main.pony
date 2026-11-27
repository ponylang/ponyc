// A lib trait's default argument is a lambda that calls lib's Helper, and the
// program calls show("c") through an l.T reference to C. The test expects
// "lib:c", and C's own copy of the method must also compile.
use l = "lib"

class C is l.T

actor Main
  new create(env: Env) =>
    let t: l.T = C
    env.out.print(t.show("c"))
