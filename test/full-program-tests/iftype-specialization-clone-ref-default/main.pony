use @pony_exitcode[None](code: I32)

class Mutable
  var x: U32

  new create(x': U32) =>
    x = x'

actor Main
  new create(env: Env) =>
    // Array.clone() returns ref when the element type is not val.
    // A class with default ref cap is not val, so the default applies.
    let arr = Array[Mutable]
    arr.push(Mutable(1))
    let cloned = arr.clone()
    cloned.push(Mutable(2))
    @pony_exitcode(cloned.size().i32())
