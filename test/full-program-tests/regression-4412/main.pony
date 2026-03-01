use @pony_exitcode[None](code: I32)

// As long as #4412 is fixed, this program will compile.

actor Main
  new create(env: Env) =>
    // A union type with a tuple is required to trigger the crash
    let z: (I32 | (I32, I32)) = (123, 42)

    match z
    // This is the line that triggered the crash due to the extra parens around
    // the let.
    | (123, (let x: I32)) => @pony_exitcode(x)
    end
