use "collections"
use "time"

actor _TestRunner
  """
  Per unit test actor that runs the test and keeps the log for it.

  Also drives property-based tests: when a property test calls
  `_start_property`, this actor enters property mode and drives the
  sample loop, shrinking, and regression replay using recursive
  behaviours.
  """

  let _ponytest: PonyTest
  let _id: USize
  let _group: _Group
  let _test: UnitTest iso
  let _log_verbose: Bool
  let _env: Env
  let _timers: Timers
  let _helper: TestHelper
  var _test_log: Array[String] iso = recover Array[String] end
  embed _expect_actions: Array[String] = Array[String]
  embed _disposables: Array[DisposableActor] = Array[DisposableActor]
  var _pass: Bool = false
  var _fun_finished: Bool = false
  var _is_long_test: Bool = false
  var _completed: Bool = false
  var _tearing_down: Bool = false
  var _test_timers: Array[Timer tag] = Array[Timer tag]

  // Property mode state
  var _property_exec: (_PropertyExecution | None) = None
  embed _property_queue: Array[_PropertyExecution iso] = Array[_PropertyExecution iso]
  var _prop_sample_id: USize = 0
  embed _prop_actions: Set[String] = Set[String]
  var _prop_sample_pass: Bool = true

  new create(
    ponytest: PonyTest,
    id: USize,
    test: UnitTest iso,
    group: _Group,
    verbose: Bool,
    env: Env,
    timers: Timers)
  =>
    """
    Create a new test runner.
    ponytest - The authority we report everything to.
    id - Test identifier needed when reporting to ponytest.
    test - The test to run.
    group - The group this test is in, which must be notified when we finish.
    env - The system environment, which is made available to tests.
    timers - The timer group we use to set long test timeouts.
    """
    _ponytest = ponytest
    _id = id
    _test = consume test
    _group = group
    _log_verbose = verbose
    _env = env
    _timers = timers
    _helper = TestHelper._create(this, _env)

  be run() =>
    """
    Run our test.
    """
    _pass = true
    _ponytest._test_started(_id)

    try
      _test.set_up(_helper)?
      try
        _test(_helper)?
      else
        log("Test threw an error", false)
        _pass = false
      end
    else
      log("Test threw an error during set_up", false)
      _pass = false
    end

    // Send ourselves a message to allow helper messages to reach us first.
    _finished()

  be log(msg: String, verbose: Bool) =>
    """
    Log the given message.

    The verbose parameter allows messages to be printed only when the --verbose
    command line option is used.

    Logs are printed one test at a time to avoid interleaving log lines from
    concurrent tests.
    """
    _log(msg, verbose)

  be fail(msg: String) =>
    """
    Flag the test as having failed.
    """
    _pass = false
    _log(msg, false)

  be complete(success: Bool, sample_id: USize = USize.max_value()) =>
    """
    MUST be called by each long test to indicate the test has finished, unless
    a timeout occurs.

    The "success" parameter specifies whether the test succeeded. However if
    the test has already been flagged as failing, then the test is considered a
    failure, regardless of the value of this parameter.

    Once this is called tear_down() may be called at any time.

    When called from a property sample (sample_id != USize.max_value()),
    completes the current sample rather than the whole test.
    """
    if sample_id != USize.max_value() then
      _property_sample_complete(success, sample_id)
      return
    end

    if not success then
      _pass = false
      _log("Complete(false) called", false)
    else
      _log("Complete(true) called", true)
    end

    for timer in _test_timers.values() do
      _timers.cancel(timer)
    end
    _test_timers.clear()

    _completed = true
    _tear_down()

  be expect_action(name: String, sample_id: USize = USize.max_value()) =>
    """
    Can be called in a long test to set up expectations for one or more actions
    that, when all completed, will complete the test.

    This pattern is useful for cases where you have multiple things that need
    to happen to complete your test, but don't want to have to collect them
    all yourself into a single actor that calls the complete method.
    """
    if sample_id != USize.max_value() then
      if sample_id != _prop_sample_id then
        _log(
          "unexpected expect action \"" + name +
            "\" for sample " + sample_id.string() +
            ". Currently at sample " + _prop_sample_id.string(),
          true)
        return
      end
      _log("Action expected: " + name, true)
      _prop_actions.set(name)
      return
    end
    _log("Action expected: " + name, true)
    _expect_actions.push(name)

  be complete_action(
    name: String,
    success: Bool,
    sample_id: USize = USize.max_value())
  =>
    """
    MUST be called for each action expectation that was set up in a long test
    to fulfill the expectations. Any expectations that are still outstanding
    when the long test timeout runs out will be printed by name when it fails.

    Completing all outstanding actions is enough to finish the test. There's no
    need to also call the complete method when the actions are finished.

    Calling the complete method will finish the test immediately, without
    waiting for any outstanding actions to be completed.

    Completing an action with success = false will cause the entire test to
    fail immediately, without waiting the rest of the outstanding actions.
    The name of the failed action will be included in the failure output.
    """
    if sample_id != USize.max_value() then
      _property_action_complete(name, success, sample_id)
      return
    end

    if success then
      _log("Action completed: " + name, true)
    else
      _log("Action failed: " + name, false)
      complete(false)
      return
    end

    for (i, action) in _expect_actions.pairs() do
      if action == name then
        try _expect_actions.delete(i)? else _Unreachable() end
        break
      end
    end

    if _expect_actions.size() == 0 then
      complete(true)
    end

  be dispose_when_done(
    disposable: DisposableActor,
    sample_id: USize = USize.max_value())
  =>
    """
    Pass a disposable actor to be disposed of when the test is complete.
    The actor will be disposed no matter whether the test succeeds or fails.

    If the test is already tearing down, the actor will be disposed immediately.
    """
    if sample_id != USize.max_value() then
      if sample_id != _prop_sample_id then
        _log("Unexpected dispose_when_done for sample " +
          sample_id.string() + ". Currently at sample " +
          _prop_sample_id.string(), true)
        disposable.dispose()
        return
      end
    end
    if _tearing_down then
      disposable.dispose()
    else
      _disposables.push(disposable)
    end

  be _finished() =>
    """
    Called when the test function completes.
    If long_test() is going to be called, it must have been by now.
    """
    if not _is_long_test then
      _log("Short test finished", true)
      _completed = true
    end

    _fun_finished = true
    _tear_down()

  be long_test(timeout: U64) =>
    """
    The test has been flagged as a long test.
    """
    if not _is_long_test then
      _is_long_test = true
      _log("Long test, timeout " + timeout.string(), true)

      if _completed then
        // We've already completed, don't start the timer
        return
      end

      let timer =
        Timer(
          object iso
            let _runner: _TestRunner = this

            fun apply(timer: Timer, count: U64): Bool =>
              _runner._timeout()
              false

            fun cancel(timer: Timer) => None
          end,
          timeout)
      _test_timers.push(timer)
      _timers(consume timer)
    else
      _log("Attempt to register duplicate long test for " + _test.name(), true)
    end

  be _timeout() =>
    """
    Called when the long test timeout expires.
    """
    if _completed then
      // Test has already completed, ignore timeout.
      return
    end

    _log("Test timed out without completing", false)
    for action in _expect_actions.values() do
      _log("Action never completed: " + action, false)
    end
    _pass = false
    _completed = true
    _test.timed_out(_helper)
    _tear_down()

  fun ref _log(msg: String, verbose: Bool) =>
    """
    Write the given message direct to our log.
    """
    if not verbose or _log_verbose then
      _test_log.push(msg)
    end

  fun ref _tear_down() =>
    """
    Check if the test has finished and tear it down if necessary.
    """
    if _fun_finished and _completed and not _tearing_down then
      // We're ready for tear down.
      _log("Tearing down test", true)
      _tearing_down = true
      _test.tear_down(_helper)

      // Dispose all collected disposable actors.
      for disposable in _disposables.values() do
        disposable.dispose()
      end

      // Send ourselves a message to allow helper messages to reach us first.
      _close()
    end

  be _close() =>
    """
    Close down this test and send a report.
    """
    let complete_log = _test_log = recover Array[String] end
    _ponytest._test_complete(_id, _pass, consume complete_log)

    _group._test_complete(this)

  // Property mode support

  be _start_property(exec: _PropertyExecution iso) =>
    if _property_exec isnt None then
      _property_queue.push(consume exec)
      return
    end
    _property_exec = consume exec
    _prop_sample_id = 0
    _prop_sample_pass = true
    _next_property_sample()

  be _next_property_sample() =>
    match _property_exec
    | let exec: _PropertyExecution =>
      let helper = TestHelper._create_sample(this, _env, _prop_sample_id)
      if exec.check_regression(helper) then
        if exec.generator_failed() then
          fail(exec.error_message())
          _property_queue.clear()
          complete(false)
        else
          _next_property_sample()
        end
        return
      end

      if not exec.has_more_samples() then
        _property_finish(exec)
        return
      end

      _prop_sample_id = _prop_sample_id + 1
      _prop_actions.clear()
      _prop_sample_pass = true

      let sample_helper =
        TestHelper._create_sample(this, _env, _prop_sample_id)
      exec.run_sample(sample_helper)

      if exec.generator_failed() then
        fail(exec.error_message())
        _property_queue.clear()
        complete(false)
      elseif not exec.last_sample_passed() then
        _property_handle_failure(exec)
      else
        exec.sample_passed()
        _next_property_sample()
      end
    end

  fun ref _property_sample_complete(success: Bool, sample_id: USize) =>
    if sample_id != _prop_sample_id then
      _log(
        "unexpected sample complete for sample " +
          sample_id.string() +
          ". Currently at sample " +
          _prop_sample_id.string(),
        true)
      return
    end

    match _property_exec
    | let exec: _PropertyExecution =>
      if not success then
        _pass = false
        _prop_sample_pass = false
        _property_handle_failure(exec)
      else
        exec.sample_passed()
        _next_property_sample()
      end
    end

  fun ref _property_action_complete(
    name: String,
    success: Bool,
    sample_id: USize)
  =>
    if sample_id != _prop_sample_id then
      _log(
        "unexpected action \"" + name +
          "\" for sample " + sample_id.string() +
          ". Currently at sample " + _prop_sample_id.string(),
        true)
      return
    end

    if success then
      _log("Action completed: " + name, true)
    else
      _log("Action failed: " + name, false)
      _property_sample_complete(false, sample_id)
      return
    end

    try
      _prop_actions.extract(name)?
      if _prop_actions.size() == 0 then
        _property_sample_complete(true, sample_id)
      end
    else
      _log(
        "Action '" + name +
          "' finished unexpectedly at sample " +
          sample_id.string() + ". ignoring.",
        true)
    end

  fun ref _property_handle_failure(exec: _PropertyExecution) =>
    exec.sample_failed()
    _prop_actions.clear()

    if exec.needs_shrink() then
      exec.begin_shrink()
      _property_shrink(exec)
    else
      _log("no choices recorded, cannot shrink", false)
      exec.save_regression()
      exec.report()
      fail(
        "Property failed for sample " + exec.sample_repr() +
          " (after 0 shrinks)")
      _property_queue.clear()
      complete(false)
    end

  be _property_shrink_step() =>
    match _property_exec
    | let exec: _PropertyExecution =>
      _property_shrink(exec)
    end

  fun ref _property_shrink(exec: _PropertyExecution) =>
    _prop_sample_id = _prop_sample_id + 1
    _prop_actions.clear()
    _prop_sample_pass = true

    let helper = TestHelper._create_sample(this, _env, _prop_sample_id)
    exec.run_shrink_candidate(helper)

    if exec.shrink_exhausted() then
      exec.save_regression()
      exec.report()
      let repr = exec.sample_repr()
      let rounds = exec.shrink_reductions()
      fail(
        "Property failed for sample " + repr +
          " (after " + rounds.string() + " shrinks)")
      _property_queue.clear()
      complete(false)
    else
      _property_shrink_step()
    end

  fun ref _property_finish(exec: _PropertyExecution) =>
    exec.report()
    if not exec.coverage_passed() then
      fail("Property failed: insufficient coverage")
      _property_queue.clear()
      complete(false)
      return
    end
    if _property_queue.size() > 0 then
      try
        let next = _property_queue.shift()?
        _property_exec = consume next
        _prop_sample_id = 0
        _prop_sample_pass = true
        _next_property_sample()
      end
    else
      complete(true)
    end

  be _property_classify(label: String, sample_id: USize) =>
    if sample_id != _prop_sample_id then return end
    match _property_exec
    | let exec: _PropertyExecution =>
      exec.classify(label)
    end

  be _property_tabulate(
    heading: String,
    label: String,
    sample_id: USize)
  =>
    if sample_id != _prop_sample_id then return end
    match _property_exec
    | let exec: _PropertyExecution =>
      exec.tabulate(heading, label)
    end

  be _property_cover(
    condition: Bool,
    label: String,
    min_pct: F64,
    sample_id: USize)
  =>
    if sample_id != _prop_sample_id then return end
    match _property_exec
    | let exec: _PropertyExecution =>
      exec.cover(condition, label, min_pct)
    end
