// A generic behaviour inherited from a trait by an actor with a type
// parameter of the same name (`R`).
use @pony_exitcode[None](code: I32)

trait tag Recv
  be got[R: Stringable val](r: R, out: Out) => out.check(r.string())

actor Out
  be check(s: String) => if s == "5" then @pony_exitcode(0) end

actor Holder[R: Any val] is Recv

actor Main
  new create(env: Env) =>
    // Out.check sets the exit code back to 0 only when got passes it "5".
    @pony_exitcode(1)
    Holder[None].got[U8](5, Out)
