// An object literal inside a lib trait default calls @labs, and main declares
// @labs with a different return type. The test expects the FFI-in-default
// error in lib and the failed module load, however main declares @labs.
use l = "lib"
use @labs[U8](x: I64)
class C is l.T
actor Main
  new create(env: Env) => env.out.print(C.f().g().string() + " " + @labs(-7).string())
