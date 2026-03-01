use "collections"
use "itertools"
use "time"

actor _Coordinator is _TesterNotify
  let _env: Env
  let _options: _Options
  let _definitions: Array[_TestDefinition] val
  let _tests_to_run: List[_TestDefinition] = List[_TestDefinition]
  let _num_to_run: USize
  let _success: Map[String, Bool]
  let _timers: Timers

  var _cur_output_name: (String | None) = None
  let _tester_outputs: Map[String, Array[String] ref] =
    Map[String, Array[String] ref]

  new create(env: Env, options: _Options,
    definitions: Array[_TestDefinition] val)
  =>
    _env = env
    _options = options
    _definitions = definitions

    if _options.only.size() > 0 then
      for definition in _definitions.values() do
        if _options.only.contains(definition.name) then
          _tests_to_run.push(definition)
        end
      end
    else
      _tests_to_run.append(definitions)
    end
    _num_to_run = _tests_to_run.size()
    _success = Map[String, Bool](_num_to_run)
    _timers = Timers

    let start_message = recover val
      var m: String ref = "Running " + _num_to_run.string() + " tests"

      if _options.debug then
        m = m + " compiled in debug mode."
      else
        m = m + " compiled in release mode."
      end
      m
    end

    _env.out.print(_Colors.info(start_message, true))

    for i in Range(0, _options.max_parallel) do
      _run_test()
    end


  be _run_test() =>
    try
      let definition = _tests_to_run.shift()?
      _Tester(_env, _timers, _options, definition, this)
    end

  be print(tester_name: String, str: String) =>
    match _cur_output_name
    | let cur_name: String =>
      if cur_name == tester_name then
        _env.out.print(str)
      else
        let array = try
          _tester_outputs(tester_name)?
        else
          _tester_outputs.insert(tester_name, Array[String])
        end
        array.push(str)
      end
    else
      _cur_output_name = tester_name
      _flush_outputs(tester_name)
      _env.out.print(str)
    end

  be succeeded(name: String) =>
    _finalize_outputs(name)
    _success(name) = true
    _check_done()

  be failed(name: String) =>
    _finalize_outputs(name)
    _success(name) = false
    _check_done()

  fun ref _finalize_outputs(name: String) =>
    let pick_new =
      match \exhaustive\ _cur_output_name
      | let cur_name: String if cur_name == name =>
        true
      | None =>
        true
      else
        false
      end

    if pick_new then
      // flush all finished tests
      let keys = Array[String](_tester_outputs.size())
        .>concat(_tester_outputs.keys())

      for key in keys.values() do
        if _success.contains(key) then
          _flush_outputs(key)
        end
      end

      // pick a new current
      _cur_output_name =
        try
          let next_name = _tester_outputs.keys().next()?
          _flush_outputs(next_name)
          next_name
        end
    end

  fun ref _flush_outputs(name: String) =>
    try
      for str in _tester_outputs(name)?.values() do
        _env.out.print(str)
      end
      _tester_outputs.remove(name)?
    end

  fun ref _check_done(): Bool =>
    if _success.size() >= _num_to_run then
      // print buffered outputs
      for array in _tester_outputs.values() do
        for str in array.values() do _env.out.print(str) end
      end

      // collect the number of successes and failures
      (let num_succeeded: USize, let num_failed: USize) =
        Iter[Bool](_success.values())
          .fold[(USize, USize)]((0, 0),
            {(sum, s) =>
              if s then (sum._1 + 1, sum._2) else (sum._1, sum._2 + 1) end
            })

      // print summary
      _env.out.print(_Colors.info("", true))
      if num_succeeded > 0 then
        _env.out.print(_Colors.pass(num_succeeded.string() + " test(s)."))
      end
      if num_failed > 0 then
        _env.out.print(_Colors.fail(num_failed.string()
          + " test(s), listed below:"))
        for (name, succeeded') in _success.pairs() do
          if not succeeded' then
            _env.out.print(_Colors.fail(name))
          end
        end
      end

      // clean up
      if num_succeeded == _num_to_run then
        _env.exitcode(0)
      else
        _env.exitcode(1)
      end

      _timers.dispose()
      true
    else
      _run_test()
      false
    end
