use "logger"

actor Main
  new create(env: Env) =>
    // get options
    let options =
      try
        _CliOptions(env)?
      else
        return
      end

    let logger =
      StringLogger(if options.verbose then Info else Warn end, env.out)

    // get test definitions
    match _TestDefinitions(env.root, options.path, logger)
    | let test_definitions: Array[_TestDefinition] =>
    end
