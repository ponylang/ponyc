use "time"

actor Main
  var _num_completed: USize = 0

  new create(env: Env) =>
    let options =
      try
        _CliOptions(env)?
      else
        return
      end

    try
      let td = _TestDefinitions(options.verbose, options.exclude, env.out,
        env.err)
      match td.find(env.root as AmbientAuth, options.test_path)
      | let test_definitions: Array[_TestDefinition] val =>
        _Coordinator(env, options, test_definitions)
      else
        error
      end
    else
      env.exitcode(1)
    end
