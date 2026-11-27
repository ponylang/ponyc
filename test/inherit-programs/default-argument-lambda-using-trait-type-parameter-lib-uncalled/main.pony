// A lib trait's default argument is a lambda that uses the trait's type
// parameter A, and C provides l.T[U8] without calling the method. The test
// expects the program to compile and print "built".
use l = "lib"

class C is l.T[U8]

actor Main
  new create(env: Env) =>
    env.out.print("built")
