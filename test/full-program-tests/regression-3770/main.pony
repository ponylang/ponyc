actor Main
  new create(env: Env) =>
    let r1 = test_then[U8]()
    let r2 = test_else[None]()

    match (r1, r2)
    | (U8(0), None) => None
    else
      env.exitcode(1)
    end

  fun test_then[T: U8](): (U8 | None) =>
    iftype T <: U8 then
      0
    end

  fun test_else[T: None](): (U8 | None) =>
    iftype T <: U8 then
      0
    else
      None
    end
