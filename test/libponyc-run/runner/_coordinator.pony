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
    _timers = Timers

    _env.out.print(_Colors.info("Running " + _num_to_run.string()
      + " tests.", true))

    if _num_to_run > 0 then
      if _options.sequential then
        _run_test()
      else
        let timer = Timer(_CoordinatorTimerNotify(this), 0,
          options.interval_ms * 1_000_000)
        _timers(consume timer)
      end
    end

  be _run_test() =>
    try
      let num_left = _tests_to_run.size()
      let num_started = _num_to_run - num_left
      let num_in_flight = num_started - _success.size()
      if (num_left > 0) and (num_in_flight < _options.max_parallel) then
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

  fun _check_done(): Bool =>
    if _success.size() >= _num_to_run then
      (let num_succeeded: USize, let num_failed: USize) =
        Iter[Bool](_success.values())
          .fold[(USize, USize)]((0, 0),
            {(sum, s) =>
              if s then (sum._1 + 1, sum._2) else (sum._1, sum._2 + 1) end
            })

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

      if num_succeeded == _num_to_run then
        _env.exitcode(0)
      else
        _env.exitcode(1)
      end

      _timers.dispose()
      true
    elseif _options.sequential then
      _run_test()
      false
    else
      false
    end

class iso _CoordinatorTimerNotify is TimerNotify
  let _coordinator: _Coordinator tag

  new iso create(coordinator: _Coordinator tag) =>
    _coordinator = coordinator

  fun ref apply(t: Timer, count: U64): Bool =>
    _coordinator._run_test()
    true
