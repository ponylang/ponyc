use "files"
use "time"

actor Main
  var _num_completed: USize = 0

  new create(env: Env) =>
    let options =
      try
        _CliOptions(env)?
      else
        env.exitcode(1)
        return
      end

    var exit_code = I32(0)
    if FilePath(FileAuth(env.root), options.output).exists() then
      let td =
        _TestDefinitions(
          options.verbose, options.exclude, env.out, env.err)
      match td.find(FileAuth(env.root), options.test_path)
      | let test_definitions: Array[_TestDefinition] val =>
        // A misspelled --only name would otherwise give a run of fewer tests
        // that still exits 0.
        let unmatched = _unmatched_only_names(options, test_definitions)
        if unmatched.size() == 0 then
          _Coordinator(env, options, test_definitions)
        else
          for name in unmatched.values() do
            env.err.print(_Colors.err("--only names no test: " + name))
          end
          exit_code = 1
        end
      else
        env.err.print(_Colors.err("Unable to get test definitions."))
        exit_code = 1
      end
    else
      env.err.print(
        _Colors.err(options.output + ": Output directory does not exist."))
      exit_code = 1
    end
    env.exitcode(exit_code)

  fun _unmatched_only_names(
    options: _Options,
    definitions: Array[_TestDefinition] val)
    : Array[String]
  =>
    """
    The names given to --only that match no test directory.
    """
    let unmatched = Array[String]
    for name in options.only.values() do
      var found = false
      for definition in definitions.values() do
        if definition.name == name then
          found = true
          break
        end
      end
      if not found then unmatched.push(name) end
    end
    unmatched
