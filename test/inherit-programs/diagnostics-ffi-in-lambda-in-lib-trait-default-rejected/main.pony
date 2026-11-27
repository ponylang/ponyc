// A lambda inside a lib trait default calls the FFI function @labs. FFI calls
// aren't allowed anywhere in a trait's text, lambdas included, so the test
// expects that error in lib and the failed load of the lib module.
use l = "lib"
class C is l.T
actor Main
  new create(env: Env) => env.out.print(C.f()().string())
