actor Main
  new create(env: Env) => meaning_of_life()
  fun meaning_of_life(): (String, (String|U32)) =>
    let x = U32(1)
    ("hi", "hi")
