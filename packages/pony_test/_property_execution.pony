use "collections"

interface ref _PropertyExecution
  """
  Non-generic interface for property execution, driven by _TestRunner.
  """
  fun ref run_sample(h: TestHelper)
  fun ref last_sample_passed(): Bool
  fun ref generator_failed(): Bool
  fun ref error_message(): String
  fun ref sample_passed()
  fun ref sample_failed()
  fun ref has_more_samples(): Bool
  fun ref needs_shrink(): Bool
  fun ref begin_shrink()
  fun ref run_shrink_candidate(h: TestHelper)
  fun ref shrink_exhausted(): Bool
  fun ref report()
  fun ref coverage_passed(): Bool
  fun ref classify(label: String)
  fun ref tabulate(heading: String, label: String)
  fun ref cover(condition: Bool, label: String, min_pct: F64)
  fun ref check_regression(h: TestHelper): Bool
  fun ref is_shrinking(): Bool
  fun ref sample_repr(): String
  fun ref shrink_reductions(): USize
  fun ref save_regression()

class ref _PropertyExec[T] is _PropertyExecution
  """
  Generic property execution engine. Held as a `ref` field inside
  `_TestRunner`, driven by `_TestRunner`'s own behaviours.
  """
  let _prop: Property[T]
  let _params: PropertyParams
  let _gen: Generator[T]
  let _logger: TestHelper
  embed _engine: _GenerationEngine
  var _sample_repr: String = ""
  var _pass: Bool = true
  var _generator_failed: Bool = false
  var _error_msg: String = ""
  var _current_sample: USize = 0
  var _shrinking: Bool = false
  var _shrink_failed_repr: String = ""
  var _shrink_done: Bool = false

  new create(
    prop: Property[T] iso,
    params: PropertyParams,
    logger: TestHelper,
    env: Env,
    classification_notify: (ClassificationNotify | None) = None)
  =>
    _prop = consume prop
    _params = params
    _logger = logger
    _gen = _prop.gen()
    _engine =
      _GenerationEngine(
        params, _prop.name(), env, classification_notify)

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

    (sample, _sample_repr) = _Stringify.apply[T](consume sample)
    _logger.log(
      "Replaying stored regression for \"" + _prop.name() + "\"")

    _pass = true

    try
      _prop.property(consume sample, h)?
    else
      _pass = false
      _generator_failed = true
      _error_msg =
        "Stored regression still fails for \"" + _prop.name() +
          "\": " + _sample_repr
      return
    end

    if not _pass then
      _generator_failed = true
      _error_msg =
        "Stored regression still fails for \"" + _prop.name() +
          "\": " + _sample_repr
    else
      _logger.log(
        "Regression replay passed for \"" + _prop.name() +
          "\" — clearing stored regression")
      _engine.clear_regression(_logger)
    end

  fun ref run_sample(h: TestHelper) =>
    _generator_failed = false
    _error_msg = ""
    _engine.begin_sample()

    var sample: T =
      try
        _generate_with_retry(_params.max_generator_retries)?
      else
        _engine.reset_rnd()
        _engine.report_health_checks(_logger)
        _pass = false
        _generator_failed = true
        _error_msg =
          "Unable to generate samples from the given iterator, tried " +
          _params.max_generator_retries.string() + " times." +
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
    _shrink_failed_repr = _sample_repr
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
    var sample: T =
      try
        _gen.generate(_engine.rnd())?
      else
        return
      end

    let new_choices = _engine.trim_to_consumed(candidate)

    (sample, let current_repr) = _Stringify.apply[T](consume sample)
    let new_spans = _engine.get_spans()

    _pass = true

    try
      _prop.property(consume sample, h)?
    else
      _engine.accept_shrink(new_choices, new_spans)
      _shrink_failed_repr = current_repr
      return
    end

    if not _pass then
      _engine.accept_shrink(new_choices, new_spans)
      _shrink_failed_repr = current_repr
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
      _sample_repr
    end

  fun ref shrink_reductions(): USize =>
    _engine.shrink_reductions()

  fun ref save_regression() =>
    _engine.save_regression(_logger)
