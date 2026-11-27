// A lib trait default calls the FFI function @labs, and a class in main
// provides the trait. FFI calls aren't allowed in default methods, so the test
// expects that error in lib and the failed load of the lib module.
use l = "lib"
class C is l.T
actor Main
  new create(env: Env) => env.out.print(C.f().string())
