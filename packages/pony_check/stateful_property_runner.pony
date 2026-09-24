use "collections"

actor StatefulPropertyRunner[S, M, Cmd: Stringable val]
  is _IPropertyRunner
  """
  Executes a StatefulProperty using recursive behaviours with
  interleaved step execution.

  Each sample: draw step count, create fresh state via factory methods,
  execute steps with interleaved draws from Randomness, check invariants,
  run final_check. On failure, the choice sequence is handed to _Shrinker
  unchanged for replay-based shrinking.
  """
  let _prop: StatefulProperty[S, M, Cmd]
  let _params: PropertyParams
  let _env: Env
  let _notify: PropertyResultNotify
  let _logger: PropertyLogger
  let _max_steps: USize
  embed _engine: _GenerationEngine
  var _current_round: _Round = _Run.create(0)
  var _pass: Bool = true
  var _cmd_trace: Array[Cmd] = Array[Cmd]
  let _expected_actions: Set[String] = Set[String]
  let _disposables: Array[DisposableActor] = Array[DisposableActor]
  var _consecutive_errors: USize = 0

  new create(
    prop: StatefulProperty[S, M, Cmd] iso,
    params: PropertyParams,
    notify: PropertyResultNotify,
    logger: PropertyLogger,
    env: Env)
  =>
    _env = env
    _prop = consume prop
    _params = params
    _notify = notify
    _logger = logger
    _max_steps = _prop.max_steps()
    _engine = _GenerationEngine(params, _prop.name(), env)
    if _max_steps == 0 then
      _notify.fail("max_steps() must be at least 1")
      _notify.complete(false)
    elseif _params.async then
      _notify.fail("StatefulProperty does not support async mode")
      _notify.complete(false)
    end

  be run() =>
    """
    Execute the property test.
    """
    if (_max_steps == 0) or _params.async then return end

    if not _engine.regression_checked() then
      _engine.mark_regression_checked()
      match _engine.load_regressions(_logger)
      | let choices: Array[_Choice val] val =>
        _replay_regression(choices)
        return
      end
    end

    if this._current_round.round() >= _params.num_samples then
      complete()
      return
    end

    _engine.begin_sample()

    let num_steps =
      try
        _engine.rnd().usize(1, _max_steps)?
      else
        _Unreachable()
        return
      end

    let ctx =
      StatefulContext[S, M](
        _prop.initial_sut(),
        _prop.initial_model())
    _cmd_trace = Array[Cmd](num_steps)

    let run_notify = recover val this~complete_run() end
    let helper =
      PropertyHelper(
        _env,
        this,
        run_notify,
        this._current_round,
        _params.string())
    _pass = true

    var failed_at_step: (USize | None) = None
    var i: USize = 0
    while i < num_steps do
      let cmd =
        try
          _prop.step(ctx, _engine.rnd(), helper)?
        else
          _engine.reset_rnd()
          _consecutive_errors = _consecutive_errors + 1
          if _consecutive_errors > _params.max_generator_retries then
            _engine.report_health_checks(_logger)
            _notify.fail(
              "Unable to generate valid commands, " +
              _consecutive_errors.string() +
              " consecutive step errors")
            _notify.complete(false)
            return
          end
          _prepare_next_round()
          run()
          return
        end
      _cmd_trace.push(cmd)

      if (failed_at_step is None) and
        (not _prop.invariant(ctx, helper))
      then
        failed_at_step = i
      end

      i = i + 1
    end

    if failed_at_step is None then
      if not _prop.final_check(ctx, helper) then
        failed_at_step = num_steps
      end
    else
      _prop.final_check(ctx, helper)
    end

    _consecutive_errors = 0
    _engine.inc_samples_run()

    match \exhaustive\ failed_at_step
    | let step: USize =>
      _engine.collect_health_metrics()
      _engine.capture_failure()

      if _engine.failing_choices().size() == 0 then
        _logger.log("no choices recorded, cannot shrink")
        _prepare_next_round()
        fail(_format_sample_repr(step), 0)
      else
        let repr = _format_sample_repr(step)
        _prepare_next_round()
        this._current_round = _Shrink.create(0)
        do_shrink(repr)
      end
    | None =>
      _engine.collect_health_metrics()
      _run_finished(this._current_round)
    end

  be _run_finished(round: _Round) =>
    if _pass then
      complete_run(round, true)
    end

  fun ref _replay_regression(choices: Array[_Choice val] val) =>
    _engine.replay(choices)

    let num_steps =
      try
        _engine.rnd().usize(1, _max_steps)?
      else
        _logger.log(
          "Stored regression stale for \"" + _prop.name() +
            "\", removing")
        _engine.clear_regression(_logger)
        run()
        return
      end

    let ctx =
      StatefulContext[S, M](
        _prop.initial_sut(),
        _prop.initial_model())
    _cmd_trace = Array[Cmd](num_steps)

    _logger.log(
      "Replaying stored regression for \"" + _prop.name() + "\"")

    let run_notify = recover val this~complete_run() end
    let helper =
      PropertyHelper(
        _env,
        this,
        run_notify,
        this._current_round,
        _params.string())

    var failed_at_step: (USize | None) = None
    var i: USize = 0
    while i < num_steps do
      let cmd =
        try
          _prop.step(ctx, _engine.rnd(), helper)?
        else
          _logger.log(
            "Stored regression stale for \"" + _prop.name() +
              "\", removing")
          _engine.clear_regression(_logger)
          run()
          return
        end
      _cmd_trace.push(cmd)

      if (failed_at_step is None) and
        (not _prop.invariant(ctx, helper))
      then
        failed_at_step = i
      end

      i = i + 1
    end

    if failed_at_step is None then
      if not _prop.final_check(ctx, helper) then
        failed_at_step = num_steps
      end
    else
      _prop.final_check(ctx, helper)
    end

    match \exhaustive\ failed_at_step
    | let step: USize =>
      _prepare_next_round()
      _notify.fail(
        "Stored regression still fails for \"" + _prop.name() +
          "\": " + _format_sample_repr(step))
      _notify.complete(false)
    | None =>
      _logger.log(
        "Regression replay passed for \"" + _prop.name() +
          "\" — clearing stored regression")
      _engine.clear_regression(_logger)
      this._expected_actions.clear()
      for disposable in Poperator[DisposableActor](this._disposables) do
        disposable.dispose()
      end
      run()
    end

  be complete_run(round: _Round, success: Bool) =>
    if this._current_round != round then
      _logger.log(
        "unexpected " +
          (if success then "finish" else "fail" end) +
          " msg for " + round.string() +
          ". expecting " +
          this._current_round.string(),
        true)
      return
    end

    _pass = success

    if not success then
      match this._current_round
      | let _: _Run => _engine.collect_health_metrics()
      end
      _engine.capture_failure()

      if _engine.failing_choices().size() == 0 then
        _logger.log("no choices recorded, cannot shrink")
        _prepare_next_round()
        fail("", 0)
      else
        let repr = _format_sample_repr(None)
        _prepare_next_round()
        this._current_round = _Shrink.create(0)
        do_shrink(repr)
      end
    else
      _prepare_next_round()
      run()
    end

  fun ref _prepare_next_round() =>
    this._current_round = this._current_round.inc()
    this._expected_actions.clear()
    for disposable in Poperator[DisposableActor](this._disposables) do
      disposable.dispose()
    end

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

  be do_shrink(failed_repr: String) =>
    """
    Begin shrinking the failing choice sequence.
    """
    _engine.begin_shrink()
    _try_next_candidate(failed_repr)

  be _try_next_candidate(failed_repr: String) =>
    let candidate =
      match _engine.next_shrink_candidate()
      | let c: Array[_Choice val] val => c
      else
        fail(failed_repr, _engine.shrink_reductions())
        return
      end

    _engine.replay(candidate)

    let num_steps =
      try
        _engine.rnd().usize(1, _max_steps)?
      else
        _try_next_candidate(failed_repr)
        return
      end

    let ctx =
      StatefulContext[S, M](
        _prop.initial_sut(),
        _prop.initial_model())
    _cmd_trace = Array[Cmd](num_steps)

    // Captures untrimmed candidate: an async h.fail() during the step
    // loop can accept it before new_choices is computed.
    let run_notify =
      recover val
        this~_shrink_candidate_result(
          failed_repr,
          "",
          candidate,
          recover val Array[_Span val] end)
      end
    let helper =
      PropertyHelper(
        _env,
        this,
        run_notify,
        this._current_round,
        _params.string())
    _pass = true

    var failed_at_step: (USize | None) = None
    var i: USize = 0
    while i < num_steps do
      let cmd =
        try
          _prop.step(ctx, _engine.rnd(), helper)?
        else
          _prepare_next_round()
          _try_next_candidate(failed_repr)
          return
        end
      _cmd_trace.push(cmd)

      if (failed_at_step is None) and
        (not _prop.invariant(ctx, helper))
      then
        failed_at_step = i
      end

      i = i + 1
    end

    if failed_at_step is None then
      if not _prop.final_check(ctx, helper) then
        failed_at_step = num_steps
      end
    else
      _prop.final_check(ctx, helper)
    end

    let new_choices = _engine.trim_to_consumed(candidate)
    let new_spans = _engine.get_spans()

    match \exhaustive\ failed_at_step
    | let _: USize =>
      let current_repr = _format_sample_repr(failed_at_step)
      _engine.accept_shrink(new_choices, new_spans)
      _prepare_next_round()
      _try_next_candidate(current_repr)
    | None =>
      _shrink_candidate_finished(
        failed_repr,
        "",
        new_choices,
        new_spans,
        this._current_round)
    end

  be _shrink_candidate_result(
    failed_repr: String,
    current_repr: String,
    new_choices: Array[_Choice val] val,
    new_spans: Array[_Span val] val,
    round: _Round,
    success: Bool)
  =>
    if round != this._current_round then return end
    if success then
      _prepare_next_round()
      _try_next_candidate(failed_repr)
    else
      _engine.accept_shrink(new_choices, new_spans)
      _prepare_next_round()
      _try_next_candidate(current_repr)
    end

  be _shrink_candidate_finished(
    failed_repr: String,
    current_repr: String,
    new_choices: Array[_Choice val] val,
    new_spans: Array[_Span val] val,
    round: _Round)
  =>
    if _pass then
      _shrink_candidate_result(
        failed_repr,
        current_repr,
        new_choices,
        new_spans,
        round,
        true)
    end

  be expect_action(name: String, round: _Round) =>
    if round != this._current_round then
      _logger.log(
        "unexpected expect action \"" + name +
          "\" call for " + round.string() +
          ". Currently at " +
          this._current_round.string(),
        true)
      return
    end
    _logger.log("Action expected: " + name)
    _expected_actions.set(name)

  be complete_action(
    name: String,
    round: _Round,
    ph: PropertyHelper)
  =>
    if round != this._current_round then
      _logger.log(
        "unexpected complete action \"" + name +
          "\" msg for " + round.string() +
          ". Currently at " +
          this._current_round.string(),
        true)
      return
    end
    _logger.log("Action completed: " + name)
    _finish_action(name, true, round, ph)

  be fail_action(
    name: String,
    round: _Round,
    ph: PropertyHelper)
  =>
    if round != this._current_round then
      _logger.log(
        "unexpected fail action \"" + name +
          "\" msg for " + round.string() +
          ". Currently at " +
          this._current_round.string(),
        true)
      return
    end
    _logger.log("Action failed: " + name)
    _finish_action(name, false, round, ph)

  fun ref _finish_action(
    name: String,
    success: Bool,
    round: _Round,
    ph: PropertyHelper)
  =>
    try
      _expected_actions.extract(name)?

      if not success then
        ph.complete(false)
      elseif _expected_actions.size() == 0 then
        ph.complete(true)
      end
    else
      _logger.log(
        "Action '" + name +
          "' finished unexpectedly at " +
          round.string() + ". ignoring.")
    end

  be classify(label: String, round: _Round) =>
    if round != this._current_round then
      _logger.log(
        "unexpected classify \"" + label +
          "\" call for " + round.string() +
          ". Currently at " +
          this._current_round.string(),
        true)
      return
    end
    match round
    | let _: _Run =>
      _engine.classify(label)
    end

  be tabulate(heading: String, label: String, round: _Round) =>
    if round != this._current_round then
      _logger.log(
        "unexpected tabulate \"" + heading + ": " + label +
          "\" call for " + round.string() +
          ". Currently at " +
          this._current_round.string(),
        true)
      return
    end
    match round
    | let _: _Run =>
      _engine.tabulate(heading, label)
    end

  be cover(condition: Bool, label: String, min_pct: F64, round: _Round) =>
    if round != this._current_round then
      _logger.log(
        "unexpected cover \"" + label +
          "\" call for " + round.string() +
          ". Currently at " +
          this._current_round.string(),
        true)
      return
    end
    match round
    | let _: _Run =>
      _engine.cover(condition, label, min_pct)
    end

  be dispose_when_done(disposable: DisposableActor, round: _Round) =>
    if round != this._current_round then
      _logger.log("Unexpected dispose_when_done for " + round.string() +
        ". Currently at " + this._current_round.string(), true)
      _logger.log("Disposing right now...", true)
      disposable.dispose()
      return
    end
    _disposables.push(disposable)

  be dispose() =>
    _dispose()

  fun ref _dispose() =>
    for disposable in Poperator[DisposableActor](_disposables) do
      disposable.dispose()
    end

  be log(msg: String, verbose: Bool = false) =>
    _logger.log(msg, verbose)

  fun ref complete() =>
    """
    Complete the property execution successfully.
    """
    _engine.report_labels(_logger)
    _engine.report_health_checks(_logger)
    if _engine.check_coverage(_logger) then
      _notify.complete(true)
    else
      _notify.fail("Property failed: insufficient coverage")
      _notify.complete(false)
    end

  fun ref fail(repr: String, shrink_rounds: USize = 0, err: Bool = false) =>
    """
    Complete the property execution while signalling failure.
    """
    _engine.report_labels(_logger)
    _engine.report_health_checks(_logger)
    _engine.save_regression(_logger)
    if err then
      _report_error(repr, shrink_rounds)
    else
      _report_failed(repr, shrink_rounds)
    end
    _notify.complete(false)

  fun _report_error(sample_repr: String, shrink_rounds: USize = 0) =>
    _notify.fail(
      "Property errored for sample " +
        sample_repr +
        " (after " +
        shrink_rounds.string() +
        " shrinks)")

  fun _report_failed(sample_repr: String, shrink_rounds: USize = 0) =>
    _notify.fail(
      "Property failed for sample " +
        sample_repr +
        " (after " +
        shrink_rounds.string() +
        " shrinks)")
