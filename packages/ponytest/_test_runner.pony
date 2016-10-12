use "time"

actor _TestRunner
  """
  Per unit test actor that runs the test and keeps the log for it.
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
  var _timer: (Timer tag | None) = None

  new create(ponytest: PonyTest, id: USize, test: UnitTest iso, group: _Group,
    verbose: Bool, env: Env, timers: Timers)
  =>
    """
    Create a new TestHelper.
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
      _test(_helper)
    else
      log("Test threw an error", false)
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

  be complete(success: Bool) =>
    """
    MUST be called by each long test to indicate the test has finished, unless
    a timeout occurs.

    The "success" parameter specifies whether the test succeeded. However if
    the test has already been flagged as failing, then the test is considered a
    failure, regardless of the value of this parameter.

    Once this is called tear_down() may be called at any time.
    """
    if not success then
      _pass = false
      _log("Complete(false) called", false)
    else
      _log("Complete(true) called", true)
    end

    try
      // Cancel timeout, if in operation.
      let timer = _timer as Timer tag
      _timers.cancel(timer)
      _timer = None
    end

    _completed = true
    _tear_down()

  be expect_action(name: String) =>
    """
    Can be called in a long test to set up expectations for one or more actions
    that, when all completed, will complete the test.

    This pattern is useful for cases where you have multiple things that need
    to happen to complete your test, but don't want to have to collect them
    all yourself into a single actor that calls the complete method.
    """
    _log("Action expected: " + name, true)
    _expect_actions.push(name)

  be complete_action(name: String, success: Bool) =>
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
    if success then
      _log("Action completed: " + name, true)
    else
      _log("Action failed: " + name, false)
      complete(false)
      return
    end

    for (i, action) in _expect_actions.pairs() do
      if action == name then
        try _expect_actions.delete(i) end
        break
      end
    end

    if _expect_actions.size() == 0 then
      complete(true)
    end

  be dispose_when_done(disposable: DisposableActor) =>
    """
    Pass a disposable actor to be disposed of when the test is complete.
    The actor will be disposed no matter whether the test succeeds or fails.

    If the test is already tearing down, the actor will be disposed immediately.
    """
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
    _is_long_test = true
    _log("Long test, timeout " + timeout.string(), true)

    if _completed then
      // We've already completed, don't start the timer
      return
    end

    let timer = Timer(object iso
      let _runner: _TestRunner = this
      fun apply(timer: Timer, count: U64): Bool => _runner._timeout(); false
      fun cancel(timer: Timer) => None
      end, timeout)
    _timer = timer
    _timers(consume timer)

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
    // First tell the ponytest that we've completed, then our group.
    // When we tell the group another test may be started. If we did that first
    // then the ponytest might report the start of that new test before the end
    // of this one, which would make it look like exclusion wasn't working.
    let complete_log = _test_log = recover Array[String] end
    _ponytest._test_complete(_id, _pass, consume complete_log)

    _group._test_complete(this)
