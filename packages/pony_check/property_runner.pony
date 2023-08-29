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
  """Represents a single execution of a property."""


interface val PropertyLogger
  fun log(msg: String, verbose: Bool = false)

interface val PropertyResultNotify
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
  """
  let _prop1: Property1[T]
  let _params: PropertyParams
  let _rnd: Randomness
  let _notify: PropertyResultNotify
  let _gen: Generator[T]
  let _logger: PropertyLogger
  let _env: Env

  // state changed during runtime
  var _current_round: _Round = _Run.create(0)
    """
    The number and kind of the round currently executed.
    Keep track of which runs/shrinks we expect to be notified about.
    """
  let _expected_actions: Set[String] = Set[String]
    """
    List of expected actions for this round.
    Will be cleared after each round.
    """
  let _disposables: Array[DisposableActor] = Array[DisposableActor]
    """
    Disosable actors that are disposed of after every round.
    """
  var _shrinker: Iterator[T^] = _EmptyIterator[T^]
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

    // verify that this is an expected call
    if this._current_round != round then
      _logger.log("unexpected " + if success then "finish" else "fail" end + " msg for " + round.string() +
        ". expecting " + this._current_round.string(), true)
      return
    end

    _pass = success // in case of sync property - signal failure

    if not success then
      // found a bad example, try to shrink it
      if not _shrinker.has_next() then
        _logger.log("no shrinks available")
        fail(_sample_repr, 0)
      else
        // prepare next round
        _prepare_next_round()
        // set rounds to shrinking
        this._current_round = _Shrink.create(0) // reset rounds for shrinking
        // start shrinking process
        do_shrink(_sample_repr)
      end
    else
      // property holds, recurse
      _prepare_next_round()
      run()
    end

  fun ref _prepare_next_round() =>
    this._current_round = this._current_round.inc()
    this._expected_actions.clear()
    for disposable in Poperator[DisposableActor](this._disposables) do
      disposable.dispose()
    end

  fun ref _generate_with_retry(max_retries: USize): ValueAndShrink[T] ? =>
    var tries: USize = 0
    repeat
      try
        return _gen.generate_and_shrink(_rnd)?
      else
        tries = tries + 1
      end
    until (tries > max_retries) end

    error

  be run() =>
    if this._current_round.round() >= _params.num_samples then
      complete() // all samples have been successful
      return
    end

    // prepare property run
    (var sample, _shrinker) =
      try
        _generate_with_retry(_params.max_generator_retries)?
      else
        // break out if we were not able to generate a sample
        _notify.fail(
          "Unable to generate samples from the given iterator, tried " +
          _params.max_generator_retries.string() + " times." +
          " (round: " + this._current_round.string() + ")")
        _notify.complete(false)
        return
      end


    // create a string representation before consuming ``sample`` with property
    (sample, _sample_repr) = _Stringify.apply[T](consume sample)
    let run_notify = recover val this~complete_run() end
    let helper = PropertyHelper(_env, this, run_notify, this._current_round, _params.string())
    _pass = true // will be set to false by fail calls

    try
      _prop1.property(consume sample, helper)?
    else
      fail(_sample_repr, 0 where err=true)
      return
    end
    // dispatch to another behavior
    // as complete_run might have set _pass already through a call to
    // complete_run
    _run_finished(this._current_round)

  be _run_finished(round: _Round) =>
    if not _params.async and _pass then
      // otherwise complete_run has already been called
      complete_run(round, true)
    end

// SHRINKING //

  be complete_shrink(failed_repr: String, last_repr: String, shrink_round: _Round, success: Bool) =>

    // verify that this is an expected call
    if this._current_round != shrink_round then
      _logger.log("unexpected " + if success then "complete" else "fail" end + " msg for " + shrink_round.string() +
        ". Currently at " + _current_round.string(), true)
      return
    end

    _pass = success // in case of sync property - signal failure

    if success then
      // we have a sample that did not fail and thus can stop shrinking
      fail(failed_repr, shrink_round.round())

    else
      // we have a failing shrink sample, recurse
      _prepare_next_round()
      do_shrink(last_repr)
    end

  be do_shrink(failed_repr: String) =>

    // shrink iters can be infinite, so we need to limit
    // the examples we consider during shrinking
    let round_num = this._current_round.round()
    if round_num == _params.max_shrink_rounds then
      fail(failed_repr, round_num)
      return
    end

    (let shrink, let current_repr) =
      try
        _Stringify.apply[T](_shrinker.next()?)
      else
        // no more shrink samples, report previous failed example
        fail(failed_repr, round_num)
        return
      end
    // callback for asynchronous shrinking or aborting on error case
    let run_notify =
      recover val
        this~complete_shrink(failed_repr, current_repr)
      end
    let helper = PropertyHelper(
      _env,
      this,
      run_notify,
      this._current_round,
      _params.string()
    )
    _pass = true // will be set to false by fail calls

    try
      _prop1.property(consume shrink, helper)?
    else
      fail(current_repr, round_num where err=true)
      return
    end
    // dispatch to another behaviour
    // to ensure _complete_shrink has been called already
    _shrink_finished(failed_repr, current_repr, this._current_round)

  be _shrink_finished(
    failed_repr: String,
    current_repr: String,
    shrink_round: _Round)
  =>
    if not _params.async and _pass then
      // directly complete the shrink run
      complete_shrink(failed_repr, current_repr, shrink_round, true)
    end

// interface towards PropertyHelper

  be expect_action(name: String, round: _Round) =>
    if round != this._current_round then
      _logger.log("unexpected expect action \"" + name + "\" call for " + round.string() +
        ". Currently at " + this._current_round.string(), true)
      return
    end
    _logger.log("Action expected: " + name)
    _expected_actions.set(name)

  be complete_action(name: String, round: _Round, ph: PropertyHelper) =>
    if round != this._current_round then
      _logger.log("unexpected complete action \"" + name + "\" msg for " + round.string() +
        ". Currently at " + this._current_round.string(), true)
      return
    end
    _logger.log("Action completed: " + name)
    _finish_action(name, true, round, ph)

  be fail_action(name: String, round: _Round, ph: PropertyHelper) =>
    if round != this._current_round then
      _logger.log("unexpected fail action \"" + name + "\" msg for " + round.string() +
        ". Currently at " + this._current_round.string(), true)
      return
    end
    _logger.log("Action failed: " + name)
    _finish_action(name, false, round, ph)

  fun ref _finish_action(name: String, success: Bool, round: _Round, ph: PropertyHelper) =>
    try
      _expected_actions.extract(name)?

      // call back into the helper to invoke the current run_notify
      // that we don't have access to otherwise
      if not success then
        ph.complete(false)
      elseif _expected_actions.size() == 0 then
        ph.complete(true)
      end
    else
      _logger.log("Action '" + name + "' finished unexpectedly at " + round.string() + ". ignoring.")
    end

  be dispose_when_done(disposable: DisposableActor, round: _Round) =>
    """
    Let us not have older rounds interfere with newer ones, 
    thus dispose directly.
    """
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
      "Property errored for sample "
        + sample_repr
        + " (after "
        + shrink_rounds.string()
        + " shrinks)"
    )

  fun _report_failed(sample_repr: String,
    shrink_rounds: USize = 0,
    loc: SourceLoc = __loc) =>
    """
    Report a failed property and signal failure to the `PropertyResultNotify`.
    """
    _notify.fail(
      "Property failed for sample "
        + sample_repr
        + " (after "
        + shrink_rounds.string()
        + " shrinks)"
    )


class _EmptyIterator[T]
  fun ref has_next(): Bool => false
  fun ref next(): T^ ? => error

primitive _Stringify
  fun apply[T](t: T): (T^, String) =>
    """turn anything into a string"""
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

