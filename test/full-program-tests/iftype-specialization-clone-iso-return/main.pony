use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    // Array.clone() returns iso^ when element type is val.
    // Verify the caller sees the iso return type by sending the clone
    // to another actor (which requires iso).
    let arr: Array[U32] val = [1; 2; 3]
    let cloned: Array[U32] iso = arr.clone()
    let receiver = Receiver
    receiver.accept(consume cloned)

actor Receiver
  be accept(data: Array[U32] iso) =>
    @pony_exitcode(I32(1))
