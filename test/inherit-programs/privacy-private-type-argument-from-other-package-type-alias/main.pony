// Package q aliases l.T[_Mine] with its own private _Mine, and main's class
// implements that alias. The inherited say() calls name() on _Mine. Expects the
// program to compile and print "built": q wrote the type argument, so q's
// private type is allowed there.
use q = "q"

class C is q.Fooable

actor Main
  new create(env: Env) =>
    env.out.print("built")
