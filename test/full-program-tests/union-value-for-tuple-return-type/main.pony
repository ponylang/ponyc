actor Main
  new create(env: Env) =>
    try _create_tuple()._2 as Bool end

  fun _create_tuple(): (U32, (Bool | None)) =>
    let x: ((U32, Bool) | (U32, None)) = (1, true)
    x
