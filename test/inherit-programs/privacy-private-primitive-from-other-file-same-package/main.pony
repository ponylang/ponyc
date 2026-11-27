// _P is private and declared in a.pony; Main in main.pony uses it. Both files
// are in the same package, so the program compiles and prints p.
actor Main
  new create(env: Env) => env.out.print(_P.m())
