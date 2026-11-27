// lib's T.d calls Helper._secret, which is private to lib. C2 in main inherits
// d. Privacy is judged in lib, where d is written, so the program compiles
// and prints secret.
use "lib"

class C2 is T
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C2.d())
