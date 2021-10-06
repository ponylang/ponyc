use "files"
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

    var exit_code = I32(0)
    try
      let auth = env.root as AmbientAuth
      if FilePath(auth, options.output).exists() then
        let td = _TestDefinitions(options.verbose, options.exclude, env.out,
          env.err)
        match td.find(auth, options.test_path)
        | let test_definitions: Array[_TestDefinition] val =>
          _Coordinator(env, options, test_definitions)
        else
          env.err.print("Unable to get test definitions.")
          exit_code = 1
        end
      else
        env.err.print("Output directory does not exist: " + options.output)
        exit_code = 1
      end
    else
      env.err.print("Unable to get ambient auth.")
      exit_code = 1
    end
    env.exitcode(exit_code)
