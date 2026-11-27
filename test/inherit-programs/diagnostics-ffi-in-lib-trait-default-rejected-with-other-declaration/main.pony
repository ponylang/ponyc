// A lib trait default calls @labs, and main declares @labs with a different
// return type. FFI calls aren't allowed in default methods, so the test expects
// that error in lib and the failed module load, however main declares @labs.
use l = "lib"
use @labs[U8](x: I64)
class C is l.T
actor Main
  new create(env: Env) => env.out.print(C.f().string() + " " + @labs(-7).string())
