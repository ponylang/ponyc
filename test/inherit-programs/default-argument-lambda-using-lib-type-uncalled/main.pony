// A lib trait's default argument is a lambda that calls lib's Helper, and C
// provides the trait without calling the method. The test expects the program
// to compile and print "built".
use l = "lib"

class C is l.T

actor Main
  new create(env: Env) =>
    env.out.print("built")
