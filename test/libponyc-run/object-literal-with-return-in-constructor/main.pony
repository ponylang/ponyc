actor Main
  new create(env: Env) =>
    let f = object ref
      fun apply() =>
        if true then
          return None
        end
    end
