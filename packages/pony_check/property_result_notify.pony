use "debug"
use "collections"

class val _Shrink is Equatable[_Round]
  """
  An execution of a property during the shrinking process.
  """
  let _round: USize

  new val create(round': USize) =>
    _round = round'

  fun round(): USize => _round

  fun inc(): _Round =>
    _Shrink.create(this._round + 1)

  fun eq(other: box->_Round): Bool =>
    match other
    | let s: _Shrink => s._round == this._round
    else
      false
    end

  fun string(): String iso^ =>
    ("shrink(" + this._round.string() + ")").string()

class val _Run is Equatable[_Round]
  """
  An execution of a property during test mode. i.e. normal execution
  to find a sample for which the property does not hold.
  """
  let _round: USize

  new val create(round': USize) =>
    _round = round'

  fun round(): USize => _round

  fun inc(): _Round =>
    _Run.create(this._round + 1)

  fun eq(other: box->_Round): Bool =>
    match other
    | let s: _Run => s._round == this._round
    else
      false
    end

  fun string(): String iso^ =>
    ("run(" + this._round.string() + ")").string()

type _Round is (_Shrink | _Run)
  """
  Represents a single execution of a property.
  """

interface val PropertyLogger
  """
  Receives log messages from property-based tests.
  """
  fun log(msg: String, verbose: Bool = false)
    """
    Log a message during property execution.
    """

interface val PropertyResultNotify
  """
  Receives notifications about property-based test results.
  """
  fun fail(msg: String)
    """
    Called when a Property has failed (did not hold for a sample)
    or when execution raised an error.

    Does not necessarily denote completeness of the property execution,
    see `complete(success: Bool)` for that purpose.
    """

  fun complete(success: Bool)
    """
    Called when the Property execution is complete
    signalling whether it was successful or not.
    """

actor PropertyRunner[T]
  """
  Actor executing a Property1 implementation
  in a way that allows garbage collection between single
  property executions, because it uses recursive behaviours
  for looping.

  Shrinking uses choice-sequence recording and replay: the framework records
  every random decision during generation, then replays generators against
  mutated decision sequences to produce shrunk counterexamples.
  """
  let _prop1: Property1[T]
  let _params: PropertyParams
  let _rnd: Randomness
  let _notify: PropertyResultNotify
  let _gen: Generator[T]
  let _logger: PropertyLogger
  let _env: Env
  var _current_round: _Round = _Run.create(0)
  let _expected_actions: Set[String] = Set[String]
  let _disposables: Array[DisposableActor] = Array[DisposableActor]
  var _failing_choices: Array[_Choice val] val =
    recover val Array[_Choice val] end
  var _failing_spans: Array[_Span val] val =
    recover val Array[_Span val] end
  var _shrink_shrinker: (_Shrinker ref | None) = None
  var _shrink_candidates: (Iterator[Array[_Choice val] val] | None) = None
  var _sample_repr: String = ""
  var _pass: Bool = true

  new create(
    p1: Property1[T] iso,
    params: PropertyParams,
    notify: PropertyResultNotify,
    logger: PropertyLogger,
    env: Env
  ) =>
    _env = env
    _prop1 = consume p1
    _params = params
    _logger = logger
    _notify = notify
    _rnd = Randomness(_params.seed)
    _gen = _prop1.gen()

// RUNNING PROPERTIES //
  be complete_run(round: _Round, success: Bool) =>
    """
    Complete a property run.

    This behaviour is called from the PropertyHelper
    or from the actor itself.
    """
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
      _failing_choices = _rnd._get_choices()
      _failing_spans = _rnd._get_spans()

      if _failing_choices.size() == 0 then
        _logger.log("no choices recorded, cannot shrink")
        _prepare_next_round()
        fail(_sample_repr, 0)
      else
        _prepare_next_round()
        this._current_round = _Shrink.create(0)
        do_shrink(_sample_repr)
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

  fun ref _generate_with_retry(max_retries: USize): T^ ? =>
    var tries: USize = 0
    repeat
      try
        return _gen.generate(_rnd)?
      else
        tries = tries + 1
      end
    until (tries > max_retries) end

    error

  be run() =>
    """
    Execute the next property sample.
    """
    if this._current_round.round() >= _params.num_samples then
      complete()
      return
    end

    _rnd._start_recording()

    var sample: T =
      try
        _generate_with_retry(_params.max_generator_retries)?
      else
        _rnd._reset()
        _notify.fail(
          "Unable to generate samples from the given iterator, tried " +
          _params.max_generator_retries.string() + " times." +
          " (round: " + this._current_round.string() + ")")
        _notify.complete(false)
        return
      end

    (sample, _sample_repr) = _Stringify.apply[T](consume sample)
    let run_notify = recover val this~complete_run() end
    let helper =
      PropertyHelper(
        _env,
        this,
        run_notify,
        this._current_round,
        _params.string())
    _pass = true

    try
      _prop1.property(consume sample, helper)?
    else
      _failing_choices = _rnd._get_choices()
      _failing_spans = _rnd._get_spans()
      _prepare_next_round()
      fail(_sample_repr, 0 where err=true)
      return
    end
    _run_finished(this._current_round)

  be _run_finished(round: _Round) =>
    if not _params.async and _pass then
      complete_run(round, true)
    end

// SHRINKING //
  be do_shrink(failed_repr: String) =>
    """
    Shrink a failing sample using choice-sequence replay.
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
        fail(failed_repr, this._current_round.round())
        return
      end

    if not candidates.has_next() then
      fail(failed_repr, this._current_round.round())
      return
    end

    let candidate =
      try
        candidates.next()?
      else
        fail(failed_repr, this._current_round.round())
        return
      end

    _rnd._replay(candidate)
    var sample: T =
      try
        _gen.generate(_rnd)?
      else
        _rnd._reset()
        _try_next_candidate(failed_repr)
        return
      end

    let consumed = _rnd._consumed()
    let new_choices =
      if consumed < candidate.size() then
        recover val
          let trimmed = Array[_Choice val](consumed)
          try
            var i: USize = 0
            while i < consumed do
              trimmed.push(candidate(i)?)
              i = i + 1
            end
          end
          trimmed
        end
      else
        candidate
      end

    (sample, let current_repr) = _Stringify.apply[T](consume sample)
    let new_spans = _rnd._get_spans()
    _rnd._reset()

    let run_notify =
      recover val
        this~_shrink_candidate_result(
          failed_repr, current_repr, new_choices, new_spans)
      end
    let helper =
      PropertyHelper(
        _env,
        this,
        run_notify,
        this._current_round,
        _params.string())
    _pass = true

    try
      _prop1.property(consume sample, helper)?
    else
      _accept_shrink_candidate(new_choices, new_spans)
      _prepare_next_round()
      _try_next_candidate(current_repr)
      return
    end
    _shrink_candidate_finished(
      failed_repr,
      current_repr,
      new_choices,
      new_spans,
      this._current_round)

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
    if not _params.async and _pass then
      _shrink_candidate_result(
        failed_repr,
        current_repr,
        new_choices,
        new_spans,
        round,
        true)
    end

// interface towards PropertyHelper
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

  // end interface towards PropertyHelper
  fun ref complete() =>
    """
    Complete the Property execution successfully.
    """
    _notify.complete(true)

  fun ref fail(repr: String, rounds: USize = 0, err: Bool = false) =>
    """
    Complete the Property execution
    while signalling failure to the `PropertyResultNotify`.
    """
    if err then
      _report_error(repr, rounds)
    else
      _report_failed(repr, rounds)
    end
    _notify.complete(false)

  fun _report_error(sample_repr: String,
    shrink_rounds: USize = 0,
    loc: SourceLoc = __loc) =>
    """
    Report an error that happened during property evaluation
    and signal failure to the `PropertyResultNotify`.
    """
    _notify.fail(
      "Property errored for sample " +
        sample_repr +
        " (after " +
        shrink_rounds.string() +
        " shrinks)"
    )

  fun _report_failed(sample_repr: String,
    shrink_rounds: USize = 0,
    loc: SourceLoc = __loc) =>
    """
    Report a failed property and signal failure to the `PropertyResultNotify`.
    """
    _notify.fail(
      "Property failed for sample " +
        sample_repr +
        " (after " +
        shrink_rounds.string() +
        " shrinks)"
    )

primitive _Stringify
  fun apply[T](t: T): (T^, String) =>
    """
    turn anything into a string
    """
    let digest = (digestof t)
    let s =
      match t
      | let str: Stringable =>
        str.string()
      | let rs: ReadSeq[Stringable] =>
        "[" + " ".join(rs.values()) + "]"
      | (let s1: Stringable, let s2: Stringable) =>
        "(" + s1.string() + ", " + s2.string() + ")"
      | (let s1: Stringable, let s2: ReadSeq[Stringable]) =>
        "(" + s1.string() + ", [" + " ".join(s2.values()) + "])"
      | (let s1: ReadSeq[Stringable], let s2: Stringable) =>
        "([" + " ".join(s1.values()) + "], " + s2.string() + ")"
      | (let s1: ReadSeq[Stringable], let s2: ReadSeq[Stringable]) =>
        "([" + " ".join(s1.values()) + "], [" + " ".join(s2.values()) + "])"
      | (let s1: Stringable, let s2: Stringable, let s3: Stringable) =>
        "(" + s1.string() + ", " + s2.string() + ", " + s3.string() + ")"
      | ((let s1: Stringable, let s2: Stringable), let s3: Stringable) =>
        "((" + s1.string() + ", " + s2.string() + "), " + s3.string() + ")"
      | (let s1: Stringable, (let s2: Stringable, let s3: Stringable)) =>
        "(" + s1.string() + ", (" + s2.string() + ", " + s3.string() + "))"
      else
        "<identity:" + digest.string() + ">"
      end
    (consume t, consume s)
