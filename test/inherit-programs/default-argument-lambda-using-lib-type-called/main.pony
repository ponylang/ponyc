// A lib trait's default argument is a lambda that calls lib's Helper, and the
// program calls C.show("c") without that argument. The test expects "lib:c":
// Helper in the lambda resolves in lib.
use l = "lib"

class C is l.T

actor Main
  new create(env: Env) =>
    env.out.print(C.show("c"))
