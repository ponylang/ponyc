class ref _StatefulPropertyExec[S, M, Cmd: Stringable val]
  is _PropertyExecution
  """
  Stateful property execution engine. Held as a `ref` field inside
  `_TestRunner`, driven by `_TestRunner`'s own behaviours.
  """
  let _prop: StatefulProperty[S, M, Cmd]
  let _params: PropertyParams
  let _logger: TestHelper
  let _max_steps: USize
  embed _engine: _GenerationEngine
  var _current_sample: USize = 0
  var _pass: Bool = true
  var _generator_failed: Bool = false
  var _error_msg: String = ""
  var _cmd_trace: Array[Cmd] = Array[Cmd]
  var _sample_repr_str: String = ""
  var _shrinking: Bool = false
  var _shrink_failed_repr: String = ""
  var _shrink_done: Bool = false
  var _consecutive_errors: USize = 0
  var _failed_at_step: (USize | None) = None

  new create(
    prop: StatefulProperty[S, M, Cmd] iso,
    params: PropertyParams,
    logger: TestHelper,
    env: Env)
  =>
    _logger = logger
    _prop = consume prop
    _params = params
    _max_steps = _prop.max_steps()
    _engine = _GenerationEngine(params, _prop.name(), env)

  fun ref check_regression(h: TestHelper): Bool =>
    if not _engine.regression_checked() then
      _engine.mark_regression_checked()
      match _engine.load_regressions(_logger)
      | let choices: Array[_Choice val] val =>
        _run_regression(h, choices)
        return true
      end
    end
    false

  fun ref _run_regression(
    h: TestHelper,
    choices: Array[_Choice val] val)
  =>
    _engine.replay(choices)

    let num_steps: USize =
      if _max_steps == 0 then
        0
      else
        try
          _engine.rnd().usize(1, _max_steps)?
        else
          _logger.log(
            "Stored regression stale for \"" + _prop.name() +
              "\", removing")
          _engine.clear_regression(_logger)
          return
        end
      end

    let ctx =
      StatefulContext[S, M](
        _prop.initial_sut(),
        _prop.initial_model())
    _cmd_trace = Array[Cmd](num_steps)

    _logger.log(
      "Replaying stored regression for \"" + _prop.name() + "\"")

    var failed_at_step: (USize | None) = None
    var i: USize = 0
    while i < num_steps do
      let cmd =
        try
          _prop.step(ctx, _engine.rnd(), h)?
        else
          _logger.log(
            "Stored regression stale for \"" + _prop.name() +
              "\", removing")
          _engine.clear_regression(_logger)
          return
        end
      _cmd_trace.push(cmd)

      if (failed_at_step is None) and
        (not _prop.invariant(ctx, h))
      then
        failed_at_step = i
      end

      i = i + 1
    end

    if failed_at_step is None then
      if not _prop.final_check(ctx, h) then
        failed_at_step = num_steps
      end
    else
      _prop.final_check(ctx, h)
    end

    match \exhaustive\ failed_at_step
    | let step: USize =>
      _pass = false
      _generator_failed = true
      _error_msg =
        "Stored regression still fails for \"" + _prop.name() +
          "\": " + _format_sample_repr(step)
    | None =>
      _logger.log(
        "Regression replay passed for \"" + _prop.name() +
          "\" — clearing stored regression")
      _engine.clear_regression(_logger)
    end

  fun ref run_sample(h: TestHelper) =>
    _generator_failed = false
    _error_msg = ""
    _engine.begin_sample()

    let num_steps: USize =
      if _max_steps == 0 then
        0
      else
        try
          _engine.rnd().usize(1, _max_steps)?
        else
          _Unreachable()
          return
        end
      end

    let ctx =
      StatefulContext[S, M](
        _prop.initial_sut(),
        _prop.initial_model())
    _cmd_trace = Array[Cmd](num_steps)

    _pass = true

    _failed_at_step = None
    var i: USize = 0
    while i < num_steps do
      let cmd =
        try
          _prop.step(ctx, _engine.rnd(), h)?
        else
          _engine.reset_rnd()
          _consecutive_errors = _consecutive_errors + 1
          if _consecutive_errors > _params.max_generator_retries then
            _engine.report_health_checks(_logger)
            _pass = false
            _generator_failed = true
            _error_msg =
              "Unable to generate valid commands, " +
              _consecutive_errors.string() +
              " consecutive samples with step errors"
            return
          end
          _pass = false
          return
        end
      _cmd_trace.push(cmd)

      if (_failed_at_step is None) and
        (not _prop.invariant(ctx, h))
      then
        _failed_at_step = i
      end

      i = i + 1
    end

    if _failed_at_step is None then
      if not _prop.final_check(ctx, h) then
        _failed_at_step = num_steps
      end
    else
      _prop.final_check(ctx, h)
    end

    _consecutive_errors = 0
    _engine.inc_samples_run()

    match \exhaustive\ _failed_at_step
    | let step: USize =>
      _sample_repr_str = _format_sample_repr(step)
      _pass = false
    | None =>
      _sample_repr_str = ""
    end

  fun ref last_sample_passed(): Bool => _pass

  fun ref generator_failed(): Bool => _generator_failed

  fun ref error_message(): String => _error_msg

  fun ref sample_passed() =>
    _engine.collect_health_metrics()
    _current_sample = _current_sample + 1

  fun ref sample_failed() =>
    _engine.collect_health_metrics()
    _engine.capture_failure()

  fun ref has_more_samples(): Bool =>
    _current_sample < _params.num_samples

  fun ref needs_shrink(): Bool =>
    _engine.failing_choices().size() > 0

  fun ref begin_shrink() =>
    _shrinking = true
    _shrink_failed_repr = _sample_repr_str
    _shrink_done = false
    _engine.begin_shrink()

  fun ref run_shrink_candidate(h: TestHelper) =>
    let candidate =
      match _engine.next_shrink_candidate()
      | let c: Array[_Choice val] val => c
      else
        _shrink_done = true
        return
      end

    _engine.replay(candidate)

    let num_steps: USize =
      if _max_steps == 0 then
        0
      else
        try
          _engine.rnd().usize(1, _max_steps)?
        else
          return
        end
      end

    let ctx =
      StatefulContext[S, M](
        _prop.initial_sut(),
        _prop.initial_model())
    _cmd_trace = Array[Cmd](num_steps)

    _pass = true

    var failed_at_step: (USize | None) = None
    var i: USize = 0
    while i < num_steps do
      let cmd =
        try
          _prop.step(ctx, _engine.rnd(), h)?
        else
          return
        end
      _cmd_trace.push(cmd)

      if (failed_at_step is None) and
        (not _prop.invariant(ctx, h))
      then
        failed_at_step = i
      end

      i = i + 1
    end

    if failed_at_step is None then
      if not _prop.final_check(ctx, h) then
        failed_at_step = num_steps
      end
    else
      _prop.final_check(ctx, h)
    end

    let new_choices = _engine.trim_to_consumed(candidate)
    let new_spans = _engine.get_spans()

    match \exhaustive\ failed_at_step
    | let step: USize =>
      let current_repr = _format_sample_repr(step)
      _engine.accept_shrink(new_choices, new_spans)
      _shrink_failed_repr = current_repr
      _pass = false
    | None =>
      None
    end

  fun ref shrink_exhausted(): Bool =>
    _shrink_done

  fun ref report() =>
    _engine.report_labels(_logger)
    _engine.report_health_checks(_logger)

  fun ref coverage_passed(): Bool =>
    _engine.check_coverage(_logger)

  fun ref classify(label: String) =>
    _engine.classify(label)

  fun ref tabulate(heading: String, label: String) =>
    _engine.tabulate(heading, label)

  fun ref cover(condition: Bool, label: String, min_pct: F64) =>
    _engine.cover(condition, label, min_pct)

  fun ref is_shrinking(): Bool => _shrinking

  fun ref sample_repr(): String =>
    if _shrinking then
      _shrink_failed_repr
    else
      _sample_repr_str
    end

  fun ref shrink_reductions(): USize =>
    _engine.shrink_reductions()

  fun ref save_regression() =>
    _engine.save_regression(_logger)

  fun _format_sample_repr(failed_step: (USize | None)): String =>
    let s = recover iso String end
    s.append("\n  Seed: ")
    s.append(_params.seed.string())
    s.append("\n  Commands:\n")
    var i: USize = 0
    while i < _cmd_trace.size() do
      s.append("    ")
      s.append((i + 1).string())
      s.append(". ")
      try
        s.append(_cmd_trace(i)?.string())
      else
        _Unreachable()
      end
      s.append("\n")
      i = i + 1
    end
    match failed_step
    | let step: USize =>
      if step < _cmd_trace.size() then
        s.append("  Invariant failure at step ")
        s.append((step + 1).string())
      else
        s.append("  Failure in final_check")
      end
    end
    consume s
