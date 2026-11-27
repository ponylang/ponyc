// Assigns a String to a U8 local in ordinary code. The program must fail
// with one error, "right side must be a subtype of left side", and its note.
actor Main
  new create(env: Env) =>
    let x: U8 = "s"
