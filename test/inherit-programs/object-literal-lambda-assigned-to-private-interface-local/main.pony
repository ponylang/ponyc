// A lambda is assigned to a local typed with a private interface whose apply
// has the same signature. Calling the local must run the lambda and print 2.
interface _Closure
  fun apply(x: U64): U64

actor Main
  new create(env: Env) =>
    let f = {(x: U64): U64 => x + 1}
    let g: _Closure val = f
    env.out.print(g(1).string())
