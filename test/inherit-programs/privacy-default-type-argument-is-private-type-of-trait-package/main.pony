// main writes l.Box, whose type parameter defaults to lib's private _S, as
// the type argument of lib's trait. Expects "lib._S": the default type argument
// comes from lib, so the inherited bar() may call s() on it.
use l = "lib"
class C is l.T[l.Box]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar(l.MakeBox()))
