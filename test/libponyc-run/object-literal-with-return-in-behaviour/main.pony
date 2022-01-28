actor Main
  new create(env: Env) =>
    None

  be foo() =>
    let f = object ref
      fun apply() =>
        if true then
          return None
        end
    end
