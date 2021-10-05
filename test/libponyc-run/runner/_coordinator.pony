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

    if _options.verbose then
      _env.out.print("Running " + _num_to_run.string() + " tests...")
    end

    _timers = Timers

    if _options.sequential then
      _run_test()
    else
      let timer = Timer(_CoordinatorTimerNotify(this), 0,
        options.interval_ms * 1_000_000)
      _timers(consume timer)
    end

  be _run_test() =>
    try
      if _tests_to_run.size() > 0 then
        let definition = _tests_to_run.shift()?
        _Tester(_env, _timers, _options, definition, this)
      end
    end

  be succeeded(name: String) =>
    _success(name) = true
    _check_done()

  be failed(name: String) =>
    _success(name) = false
    _check_done()

  fun _check_done() =>
    if _success.size() == _num_to_run then
      (let num_succeeded: USize, let num_failed: USize) =
        Iter[Bool](_success.values())
          .fold[(USize, USize)]((0, 0),
            {(sum, s) =>
              if s then (sum._1 + 1, sum._2) else (sum._1, sum._2 + 1) end
            })

      if num_succeeded == _num_to_run then
        _env.out.print("Ran " + _num_to_run.string() + " tests: "
          + num_succeeded.string() + " succeeded.")
        _env.exitcode(0)
      else
        _env.err.print("Ran " + _num_to_run.string() + " tests: "
          + num_succeeded.string() + " succeeded, " + num_failed.string()
          + " failed.")
        _env.exitcode(1)
      end

      _timers.dispose()
    elseif _options.sequential then
      _run_test()
    end

class iso _CoordinatorTimerNotify is TimerNotify
  let _coordinator: _Coordinator tag

  new iso create(coordinator: _Coordinator tag) =>
    _coordinator = coordinator

  fun ref apply(t: Timer, count: U64): Bool =>
    _coordinator._run_test()
    true
