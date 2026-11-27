// Package q's trait Fooable fills l.T with q's private _Mine, and main's
// class implements Fooable. The inherited say() calls name() on _Mine. Expects
// the program to compile and print "built": q wrote the type argument, so q's
// private type is allowed there.
use q = "q"

class C is q.Fooable

actor Main
  new create(env: Env) =>
    env.out.print("built")
