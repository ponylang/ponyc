use "collections"
use "format"
use "time"

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
  let _rnd: Randomness
  var _current_round: _Round = _Run.create(0)
  var _pass: Bool = true
  var _failing_choices: Array[_Choice val] val =
    recover val Array[_Choice val] end
  var _failing_spans: Array[_Span val] val =
    recover val Array[_Span val] end
  var _shrink_shrinker: (_Shrinker ref | None) = None
  var _shrink_candidates: (Iterator[Array[_Choice val] val] | None) = None
  var _cmd_trace: Array[Cmd] = Array[Cmd]
  var _shrink_count: USize = 0
  let _expected_actions: Set[String] = Set[String]
  let _disposables: Array[DisposableActor] = Array[DisposableActor]
  let _label_counts: Map[String, USize] = Map[String, USize]
  let _tabulated_counts: Map[String, Map[String, USize]] =
    Map[String, Map[String, USize]]
  let _cover_requirements: Map[String, F64] = Map[String, F64]
  var _samples_run: USize = 0
  var _consecutive_errors: USize = 0
  var _total_filter_discards: USize = 0
  var _total_filter_accepts: USize = 0
  var _max_choices: USize = 0
  var _peak_sample_nanos: U64 = 0
  var _sample_start_nanos: U64 = 0

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
    _rnd = Randomness(_params.seed)
    _max_steps = _prop.max_steps()
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

    if this._current_round.round() >= _params.num_samples then
      complete()
      return
    end

    _sample_start_nanos = Time.nanos()
    _rnd._start_recording()

    let num_steps =
      try
        _rnd.usize(1, _max_steps)?
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
          _prop.step(ctx, _rnd, helper)?
        else
          _rnd._reset()
          _consecutive_errors = _consecutive_errors + 1
          if _consecutive_errors > _params.max_generator_retries then
            _report_health_checks()
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
    _samples_run = _samples_run + 1

    match \exhaustive\ failed_at_step
    | let step: USize =>
      _collect_health_metrics()
      _failing_choices = _rnd._get_choices()
      _failing_spans = _rnd._get_spans()

      if _failing_choices.size() == 0 then
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
      _collect_health_metrics()
      _run_finished(this._current_round)
    end

  be _run_finished(round: _Round) =>
    if _pass then
      complete_run(round, true)
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
      | let _: _Run => _collect_health_metrics()
      end
      _failing_choices = _rnd._get_choices()
      _failing_spans = _rnd._get_spans()

      if _failing_choices.size() == 0 then
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
    let shrinker =
      _Shrinker(
        _failing_choices,
        _failing_spans,
        _params.max_shrink_reductions)
    _shrink_shrinker = shrinker
    _shrink_candidates = shrinker.candidates()
    _try_next_candidate(failed_repr)

  be _try_next_candidate(failed_repr: String) =>
    let candidates =
      match _shrink_candidates
      | let c: Iterator[Array[_Choice val] val] => c
      else
        fail(failed_repr, _shrink_count)
        return
      end

    if not candidates.has_next() then
      fail(failed_repr, _shrink_count)
      return
    end

    let candidate =
      try
        candidates.next()?
      else
        fail(failed_repr, _shrink_count)
        return
      end

    _rnd._replay(candidate)

    let num_steps =
      try
        _rnd.usize(1, _max_steps)?
      else
        _try_next_candidate(failed_repr)
        return
      end

    let ctx =
      StatefulContext[S, M](
        _prop.initial_sut(),
        _prop.initial_model())
    _cmd_trace = Array[Cmd](num_steps)

    // Captures the untrimmed candidate so that an async h.fail() during
    // the step loop can accept it before new_choices is computed.
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
          _prop.step(ctx, _rnd, helper)?
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

    let consumed = _rnd._consumed()
    let new_choices =
      if consumed < candidate.size() then
        recover val
          let trimmed = Array[_Choice val](consumed)
          try
            var j: USize = 0
            while j < consumed do
              trimmed.push(candidate(j)?)
              j = j + 1
            end
          end
          trimmed
        end
      else
        candidate
      end
    let new_spans = _rnd._get_spans()

    match \exhaustive\ failed_at_step
    | let _: USize =>
      let current_repr = _format_sample_repr(failed_at_step)
      _accept_shrink_candidate(new_choices, new_spans)
      _shrink_count = _shrink_count + 1
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

  fun ref _accept_shrink_candidate(
    new_choices: Array[_Choice val] val,
    new_spans: Array[_Span val] val)
  =>
    match _shrink_shrinker
    | let s: _Shrinker ref =>
      s.accept(new_choices, new_spans)
    end
    _failing_choices = new_choices
    _failing_spans = new_spans

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
      _accept_shrink_candidate(new_choices, new_spans)
      _shrink_count = _shrink_count + 1
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
      _label_counts.upsert(label, 1, {(old, x) => old + x })
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
      let heading_map =
        try
          _tabulated_counts(heading)?
        else
          let m = Map[String, USize]
          _tabulated_counts(heading) = m
          m
        end
      heading_map.upsert(label, 1, {(old, x) => old + x })
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
      _cover_requirements(label) = min_pct
      if condition then
        _label_counts.upsert(label, 1, {(old, x) => old + x })
      end
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

  fun ref _collect_health_metrics() =>
    let elapsed = Time.nanos() - _sample_start_nanos
    if elapsed > _peak_sample_nanos then
      _peak_sample_nanos = elapsed
    end
    let choices_size = _rnd._choices_size()
    if choices_size > _max_choices then
      _max_choices = choices_size
    end
    (let discards, let accepts) = _rnd._count_filter_spans()
    _total_filter_discards = _total_filter_discards + discards
    _total_filter_accepts = _total_filter_accepts + accepts

  fun ref _report_health_checks() =>
    if _params.max_filter_discard_ratio > 0 then
      if _total_filter_accepts > 0 then
        let ratio =
          _total_filter_discards.f64() / _total_filter_accepts.f64()
        if ratio > _params.max_filter_discard_ratio then
          _logger.log(
            "WARNING: filter discarded " +
              _total_filter_discards.string() +
              " candidates across " + _samples_run.string() +
              " samples (" +
              Format.float[F64](ratio where fmt = FormatFix, prec = 1) +
              "x discard ratio, threshold " +
              Format.float[F64](
                _params.max_filter_discard_ratio where fmt = FormatFix,
                prec = 1) +
              "x). Generator may be too narrow for the filter predicate.")
        end
      end
    end

    if (_params.max_choice_sequence_size > 0) and
      (_max_choices > _params.max_choice_sequence_size)
    then
      _logger.log(
        "WARNING: largest choice sequence was " +
          _max_choices.string() +
          " entries (threshold " +
          _params.max_choice_sequence_size.string() +
          "). Consider simplifying the generator or reducing " +
          "collection sizes.")
    end

    if (_params.max_sample_nanos > 0) and
      (_peak_sample_nanos > _params.max_sample_nanos)
    then
      let secs =
        _peak_sample_nanos.f64() / 1_000_000_000.0
      let threshold_secs =
        _params.max_sample_nanos.f64() / 1_000_000_000.0
      _logger.log(
        "WARNING: slowest sample took " +
          Format.float[F64](secs where fmt = FormatFix, prec = 1) +
          "s (threshold " +
          Format.float[F64](
            threshold_secs where fmt = FormatFix, prec = 1) +
          "s). Consider reducing num_samples or simplifying " +
          "the property.")
    end

  fun ref complete() =>
    """
    Complete the property execution successfully.
    """
    _report_labels()
    _report_health_checks()
    if _check_coverage() then
      _notify.complete(true)
    else
      _notify.fail("Property failed: insufficient coverage")
      _notify.complete(false)
    end

  fun ref fail(repr: String, shrink_rounds: USize = 0, err: Bool = false) =>
    """
    Complete the property execution while signalling failure.
    """
    _report_labels()
    _report_health_checks()
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

  fun ref _report_labels() =>
    let has_flat = _label_counts.size() > 0
    let has_tabulated = _tabulated_counts.size() > 0
    let total = _samples_run

    if has_flat or has_tabulated then
      _logger.log("")

      if has_flat then
        let keys = Array[String](_label_counts.size())
        for k in _label_counts.keys() do
          keys.push(k)
        end
        Sort[Array[String], String](keys)
        for label in keys.values() do
          let count = try _label_counts(label)? else 0 end
          let pct = (count.f64() / total.f64()) * 100.0
          _logger.log(
            Format.float[F64](pct where fmt = FormatFix, prec = 1) +
              "% " + label +
              " (" + count.string() + "/" + total.string() + ")")
        end
      end

      if has_tabulated then
        let headings = Array[String](_tabulated_counts.size())
        for h in _tabulated_counts.keys() do
          headings.push(h)
        end
        Sort[Array[String], String](headings)
        var first_heading = true
        for heading in headings.values() do
          if has_flat or (not first_heading) then _logger.log("") end
          first_heading = false
          _logger.log(heading + ":")
          try
            let heading_map = _tabulated_counts(heading)?
            let keys = Array[String](heading_map.size())
            for k in heading_map.keys() do keys.push(k) end
            Sort[Array[String], String](keys)
            for label in keys.values() do
              let count = try heading_map(label)? else 0 end
              let pct = (count.f64() / total.f64()) * 100.0
              _logger.log(
                "  " +
                  Format.float[F64](pct where fmt = FormatFix, prec = 1) +
                  "% " + label +
                  " (" + count.string() + "/" + total.string() + ")")
            end
          else
            _Unreachable()
          end
        end
      end
    end

  fun _check_coverage(): Bool =>
    if _cover_requirements.size() == 0 then return true end
    var ok = true
    let total = _samples_run
    for (label, min_pct) in _cover_requirements.pairs() do
      let count = try _label_counts(label)? else 0 end
      let actual_pct = (count.f64() / total.f64()) * 100.0
      if actual_pct < min_pct then
        ok = false
        _logger.log(
          "Insufficient coverage: " + label + " " +
            Format.float[F64](actual_pct where fmt = FormatFix, prec = 1) +
            "% < " +
            Format.float[F64](min_pct where fmt = FormatFix, prec = 1) +
            "% required")
      end
    end
    ok
