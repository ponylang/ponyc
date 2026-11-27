// main.pony uses the private class _P declared in a.pony of the same package.
// Privacy is per package, not per file, so the program must print "p".
actor Main
  new create(env: Env) => env.out.print(_P.name())
