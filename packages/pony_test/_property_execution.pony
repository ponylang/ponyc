use "collections"

interface ref _PropertyExecution
  """
  Non-generic interface for property execution, driven by _TestRunner.
  """
  fun ref run_sample(h: PropertyHelper)
  fun ref last_sample_passed(): Bool
  fun ref has_error(): Bool
  fun ref error_message(): String
  fun ref sample_passed()
  fun ref sample_failed()
  fun ref has_more_samples(): Bool
  fun ref needs_shrink(): Bool
  fun ref begin_shrink()
  fun ref run_shrink_candidate(h: PropertyHelper)
  fun ref accept_last_shrink()
  fun ref shrink_exhausted(): Bool
  fun ref report()
  fun ref coverage_passed(): Bool
  fun ref classify(label: String)
  fun ref tabulate(heading: String, label: String)
  fun ref cover(condition: Bool, label: String, min_pct: F64)
  fun ref check_regression(h: PropertyHelper): Bool
  fun ref clear_regression()
  fun ref regression_repr(): String
  fun ref is_shrinking(): Bool
  fun ref sample_repr(): String
  fun ref shrink_reductions(): USize
  fun ref save_regression()
  fun ref max_distinct_failures(): USize
  fun ref record_failure(repr: String, shrink_count: USize)
  fun ref is_duplicate_failure(): Bool
  fun ref recorded_failure_count(): USize
  fun ref report_all_failures()
  fun ref save_all_regressions()
  fun ref resume_after_failure()

class ref _PropertyExec[T] is _PropertyExecution
  """
  Executes a `Property[T]` against generated samples, with shrinking
  and regression persistence.
  """
  let _prop: Property[T]
  let _params: PropertyParams
  let _gen: Generator[T]
  let _logger: TestHelper
  embed _engine: _GenerationEngine
  var _sample_repr: String = ""
  var _pass: Bool = true
  var _has_error: Bool = false
  var _error_msg: String = ""
  var _current_sample: USize = 0
  var _shrinking: Bool = false
  var _shrink_failed_repr: String = ""
  var _shrink_done: Bool = false
  var _shrink_last_choices: (Array[_Choice val] val | None) = None
  var _shrink_last_spans: (Array[_Span val] val | None) = None
  var _shrink_last_repr: String = ""
  embed _recorded_failures: Array[(String, USize)] =
    Array[(String, USize)]
  embed _recorded_choice_seqs: Array[Array[_Choice val] val] =
    Array[Array[_Choice val] val]
  var _regressions: Array[Array[_Choice val] val] val =
    recover val Array[Array[_Choice val] val] end
  var _regression_idx: USize = 0

  new create(
    prop: Property[T] iso,
    params: PropertyParams,
    logger: TestHelper,
    env: Env)
  =>
    _prop = consume prop
    _params = params
    _logger = logger
    _gen = _prop.gen()
    _engine = _GenerationEngine(params, _prop.name(), env)

  fun ref _generate_with_retry(max_retries: USize): T^ ? =>
    var tries: USize = 0
    repeat
      try
        return _gen.generate(_engine.rnd())?
      else
        _engine.rnd()._start_recording()
        tries = tries + 1
      end
    until (tries > max_retries) end
    error

  fun ref check_regression(h: PropertyHelper): Bool =>
    if not _engine.regression_checked() then
      _engine.mark_regression_checked()
      _regressions = _engine.load_regressions(_logger)
      _regression_idx = 0
    end
    if _regression_idx < _regressions.size() then
      try
        _run_regression(h, _regressions(_regression_idx)?)
      else
        _Unreachable()
      end
      _regression_idx = _regression_idx + 1
      return true
    end
    false

  fun ref _run_regression(
    h: PropertyHelper,
    choices: Array[_Choice val] val)
  =>
    _engine.replay(choices)
    var sample: T =
      try
        _gen.generate(_engine.rnd())?
      else
        _logger.log(
          "Stored regression stale for \"" + _prop.name() +
            "\", removing")
        _engine.clear_regression(_logger)
        return
      end

    if _engine.rnd()._replay_exhausted() then
      _logger.log(
        "Stored regression stale for \"" + _prop.name() +
          "\", removing")
      _engine.clear_regression(_logger)
      return
    end

    (sample, _sample_repr) = _Stringify.apply[T](consume sample)
    _logger.log(
      "Replaying stored regression for \"" + _prop.name() + "\"")

    _pass = true

    try
      _prop.property(consume sample, h)?
    else
      _pass = false
      _has_error = true
      _error_msg =
        "Stored regression still fails for \"" + _prop.name() +
          "\": " + _sample_repr
      return
    end
    // Don't clear the regression here. Assertion-only failures
    // arrive as deferred `fail` behaviors after this method returns.
    // The runner defers and calls `clear_regression` if no failure
    // arrived.

  fun ref run_sample(h: PropertyHelper) =>
    _has_error = false
    _error_msg = ""
    _engine.begin_sample()

    var sample: T =
      try
        _generate_with_retry(_params.max_generator_retries)?
      else
        _engine.reset_rnd()
        _engine.report_health_checks(_logger)
        _pass = false
        _has_error = true
        _error_msg =
          "Unable to generate samples from the given iterator, tried " +
          (_params.max_generator_retries + 1).string() + " times." +
          " (sample: " + _current_sample.string() + ")"
        return
      end

    _engine.inc_samples_run()
    (sample, _sample_repr) = _Stringify.apply[T](consume sample)

    _pass = true

    try
      _prop.property(consume sample, h)?
    else
      _pass = false
      return
    end

  fun ref last_sample_passed(): Bool => _pass

  fun ref has_error(): Bool => _has_error

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
    _shrink_failed_repr = _sample_repr
    _shrink_done = false
    _engine.begin_shrink()

  fun ref run_shrink_candidate(h: PropertyHelper) =>
    _shrink_last_choices = None
    _shrink_last_spans = None

    let candidate =
      match _engine.next_shrink_candidate()
      | let c: Array[_Choice val] val => c
      else
        _shrink_done = true
        return
      end

    _engine.replay(candidate)
    var sample: T =
      try
        _gen.generate(_engine.rnd())?
      else
        return
      end

    if _engine.rnd()._replay_exhausted() then
      return
    end

    let new_choices = _engine.trim_to_consumed(candidate)

    (sample, let current_repr) = _Stringify.apply[T](consume sample)
    let new_spans = _engine.get_spans()

    _shrink_last_choices = new_choices
    _shrink_last_spans = new_spans
    _shrink_last_repr = current_repr
    _pass = true

    try
      _prop.property(consume sample, h)?
    else
      _pass = false
      _engine.accept_shrink(new_choices, new_spans)
      _shrink_failed_repr = current_repr
      _shrink_last_choices = None
      _shrink_last_spans = None
    end

  fun ref accept_last_shrink() =>
    """
    Accept the most recent shrink candidate as a failure.
    Called when an assertion-only failure is detected after
    deferral — the property didn't throw but `fail` arrived.
    """
    match (_shrink_last_choices, _shrink_last_spans)
    | (let c: Array[_Choice val] val, let s: Array[_Span val] val) =>
      _engine.accept_shrink(c, s)
      _shrink_failed_repr = _shrink_last_repr
    end

  fun ref shrink_exhausted(): Bool =>
    _shrink_done

  fun ref report() =>
    _engine.report_labels(_logger)
    _engine.report_health_checks(_logger)

  fun ref coverage_passed(): Bool =>
    _engine.check_coverage(_logger)

  fun ref classify(label: String) =>
    if not _shrinking then _engine.classify(label) end

  fun ref tabulate(heading: String, label: String) =>
    if not _shrinking then _engine.tabulate(heading, label) end

  fun ref cover(condition: Bool, label: String, min_pct: F64) =>
    if not _shrinking then _engine.cover(condition, label, min_pct) end

  fun ref is_shrinking(): Bool => _shrinking

  fun ref sample_repr(): String =>
    if _shrinking then
      _shrink_failed_repr
    else
      _sample_repr
    end

  fun ref shrink_reductions(): USize =>
    _engine.shrink_reductions()

  fun ref clear_regression() =>
    if _regression_idx >= _regressions.size() then
      _logger.log(
        "All regression replays passed for \"" + _prop.name() +
          "\" — clearing stored regressions")
      _engine.clear_regression(_logger)
    else
      _logger.log(
        "Regression replay " + _regression_idx.string() + " of " +
          _regressions.size().string() + " passed for \"" +
          _prop.name() + "\"")
    end

  fun ref regression_repr(): String => _sample_repr

  fun ref save_regression() =>
    _engine.save_regression(_logger)

  fun ref max_distinct_failures(): USize =>
    _params.max_distinct_failures

  fun ref record_failure(repr: String, shrink_count: USize) =>
    _recorded_failures.push((repr, shrink_count))
    _recorded_choice_seqs.push(_engine.failing_choices())

  fun ref is_duplicate_failure(): Bool =>
    let current = _engine.failing_choices()
    for seq in _recorded_choice_seqs.values() do
      if _ChoiceSeqEq(current, seq) then return true end
    end
    false

  fun ref recorded_failure_count(): USize =>
    _recorded_failures.size()

  fun ref report_all_failures() =>
    for (repr, shrinks) in _recorded_failures.values() do
      _logger.log(
        "Property failed for sample " + repr +
          " (after " + shrinks.string() + " shrinks)", false)
    end
    _logger.log(
      _recorded_failures.size().string() +
        " distinct failures found.", false)

  fun ref save_all_regressions() =>
    var i: USize = 0
    while i < _recorded_choice_seqs.size() do
      try
        _engine.save_regression_for(
          _recorded_choice_seqs(i)?, i, _logger)
      else
        _Unreachable()
      end
      i = i + 1
    end

  fun ref resume_after_failure() =>
    _shrinking = false
    _shrink_done = false
    _shrink_last_choices = None
    _shrink_last_spans = None
    _shrink_last_repr = ""
    _shrink_failed_repr = ""
    _engine.clear_shrink_state()
    _current_sample = _current_sample + 1
