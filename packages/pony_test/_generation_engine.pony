use "collections"
use "files"
use "format"
use "time"

class ref _GenerationEngine
  """
  State and operations shared by property execution classes.

  Owns randomness, classification state, health-check metrics, shrink
  orchestration, and regression persistence.
  """
  let _params: PropertyParams
  let _name: String
  let _env: Env
  embed _rnd: Randomness
  embed _label_counts: Map[String, USize] = Map[String, USize]
  embed _tabulated_counts: Map[String, Map[String, USize]] =
    Map[String, Map[String, USize]]
  embed _cover_requirements: Map[String, F64] = Map[String, F64]
  var _samples_run: USize = 0
  var _failing_choices: Array[_Choice val] val =
    recover val Array[_Choice val] end
  var _failing_spans: Array[_Span val] val =
    recover val Array[_Span val] end
  var _shrink_shrinker: (_Shrinker ref | None) = None
  var _shrink_candidates: (Iterator[Array[_Choice val] val] | None) = None
  var _total_filter_discards: USize = 0
  var _total_filter_accepts: USize = 0
  var _max_choices: USize = 0
  var _peak_sample_nanos: U64 = 0
  var _sample_start_nanos: U64 = 0
  var _regression_dir: (FilePath | None) = None
  var _regression_checked: Bool = false

  new ref create(
    params: PropertyParams,
    name: String,
    env: Env)
  =>
    _params = params
    _name = name
    _env = env
    _rnd = Randomness(_params.seed)
    if _params.regression_db and (_name.size() > 0) then
      _regression_dir = _RegressionDb.resolve_dir(_env)
    end

  fun ref rnd(): Randomness ref => _rnd

  fun samples_run(): USize => _samples_run

  fun ref inc_samples_run() => _samples_run = _samples_run + 1

  fun ref begin_sample() =>
    _sample_start_nanos = Time.nanos()
    _rnd._start_recording()

  fun ref collect_health_metrics() =>
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

  fun ref capture_failure() =>
    _failing_choices = _rnd._get_choices()
    _failing_spans = _rnd._get_spans()

  fun ref get_spans(): Array[_Span val] val =>
    _rnd._get_spans()

  fun ref reset_rnd() =>
    _rnd._reset()

  fun failing_choices(): Array[_Choice val] val => _failing_choices

  fun failing_spans(): Array[_Span val] val => _failing_spans

  fun _label_count(label: String): USize =>
    try _label_counts(label)? else 0 end

  fun ref classify(label: String) =>
    _label_counts.upsert(label, 1, {(old, x) => old + x })

  fun ref tabulate(heading: String, label: String) =>
    let heading_map =
      try
        _tabulated_counts(heading)?
      else
        let m = Map[String, USize]
        _tabulated_counts(heading) = m
        m
      end
    heading_map.upsert(label, 1, {(old, x) => old + x })

  fun ref cover(condition: Bool, label: String, min_pct: F64) =>
    _cover_requirements(label) = min_pct
    if condition then
      _label_counts.upsert(label, 1, {(old, x) => old + x })
    end

  fun ref report_labels(logger: _PropertyLogger) =>
    let has_flat = _label_counts.size() > 0
    let has_tabulated = _tabulated_counts.size() > 0
    let total = _samples_run
    if total == 0 then return end

    if has_flat or has_tabulated then
      logger.log("")

      if has_flat then
        let keys = Array[String](_label_counts.size())
        for k in _label_counts.keys() do
          keys.push(k)
        end
        Sort[Array[String], String](keys)
        for label in keys.values() do
          let count = try _label_counts(label)? else 0 end
          let pct = (count.f64() / total.f64()) * 100.0
          logger.log(
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
          if has_flat or (not first_heading) then logger.log("") end
          first_heading = false
          logger.log(heading + ":")
          try
            let heading_map = _tabulated_counts(heading)?
            let keys = Array[String](heading_map.size())
            for k in heading_map.keys() do keys.push(k) end
            Sort[Array[String], String](keys)
            for label in keys.values() do
              let count = try heading_map(label)? else 0 end
              let pct = (count.f64() / total.f64()) * 100.0
              logger.log(
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

  fun check_coverage(logger: _PropertyLogger): Bool =>
    if _cover_requirements.size() == 0 then return true end
    let total = _samples_run
    if total == 0 then return true end
    var ok = true
    for (label, min_pct) in _cover_requirements.pairs() do
      let count = try _label_counts(label)? else 0 end
      let actual_pct = (count.f64() / total.f64()) * 100.0
      if actual_pct < min_pct then
        ok = false
        logger.log(
          "Insufficient coverage: " + label + " " +
            Format.float[F64](actual_pct where fmt = FormatFix, prec = 1) +
            "% < " +
            Format.float[F64](min_pct where fmt = FormatFix, prec = 1) +
            "% required")
      end
    end
    ok

  fun report_health_checks(logger: _PropertyLogger) =>
    _HealthCheckReporter.report(
      _params,
      _samples_run,
      _total_filter_discards,
      _total_filter_accepts,
      _max_choices,
      _peak_sample_nanos,
      logger)

  fun ref begin_shrink() =>
    let shrinker =
      _Shrinker(
        _failing_choices,
        _failing_spans,
        _params.max_shrink_reductions)
    _shrink_shrinker = shrinker
    _shrink_candidates = shrinker.candidates()

  fun ref next_shrink_candidate(): (Array[_Choice val] val | None) =>
    let candidates =
      match _shrink_candidates
      | let c: Iterator[Array[_Choice val] val] => c
      else
        return None
      end
    if not candidates.has_next() then return None end
    try
      candidates.next()?
    else
      None
    end

  fun ref replay(choices: Array[_Choice val] val) =>
    _rnd._replay(choices)

  fun ref trim_to_consumed(
    candidate: Array[_Choice val] val)
    : Array[_Choice val] val
  =>
    let c = _rnd._consumed()
    if c < candidate.size() then
      recover val
        let trimmed = Array[_Choice val](c)
        try
          var i: USize = 0
          while i < c do
            trimmed.push(candidate(i)?)
            i = i + 1
          end
        end
        trimmed
      end
    else
      candidate
    end

  fun ref accept_shrink(
    choices: Array[_Choice val] val,
    spans: Array[_Span val] val)
  =>
    match _shrink_shrinker
    | let s: _Shrinker ref =>
      s.accept(choices, spans)
    end
    _failing_choices = choices
    _failing_spans = spans

  fun shrink_reductions(): USize =>
    match _shrink_shrinker
    | let s: _Shrinker box => s.reductions()
    else
      0
    end

  fun regression_checked(): Bool => _regression_checked

  fun ref mark_regression_checked() =>
    _regression_checked = true

  fun ref load_regressions(logger: _PropertyLogger)
    : (Array[_Choice val] val | None)
  =>
    match _regression_dir
    | let dir: FilePath =>
      _RegressionDb.load(dir, _name, logger)
    else
      None
    end

  fun ref save_regression(logger: _PropertyLogger) =>
    match _regression_dir
    | let dir: FilePath =>
      if _failing_choices.size() > 0 then
        _RegressionDb.save(dir, _name, _failing_choices, logger)
      end
    end

  fun ref clear_regression(logger: _PropertyLogger) =>
    match _regression_dir
    | let dir: FilePath =>
      _RegressionDb.clear(dir, _name, logger)
    end
