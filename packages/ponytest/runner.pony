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
  var _pass: Bool = false
  var _completed: Bool = false
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
    var pass: TestResult = false
    _ponytest._test_started(_id)

    try
      // If the test throws then pass will keep its value of false, no extra
      // processing is needed.
      pass = _test(_helper)
    else
      log("Test threw an error", false)
    end

    match pass
    | let p: Bool =>
      // This isn't a long test, ie the test is complete.
      // We need to process any pending log and failure messages. Send
      // ourselves a complete() message so we know when we're done.
      complete(p)
    | let t: U64 =>
      // This is a long test. Leave it going, but setup a timeout.
      let timer = Timer(object iso
        let _runner: _TestRunner = this
        fun cancel(timer: Timer) => None
        fun apply(timer: Timer, count: U64): Bool => _runner._timeout(); false
        fun cancel(timer: Timer) => None
        end, t)
      _timer = timer
      _timers(consume timer)
    end

  be log(msg: String, verbose: Bool) =>
    """
    Log the given message.

    The verbose parameter allows messages to be printed only when the --verbose
    command line option is used.

    Logs are printed one test at a time to avoid interleaving log lines from
    concurrent tests.
    """
    if not verbose or _log_verbose then
      _test_log.push(msg)
    end

  be fail() =>
    """
    Flag the test as having failed.
    """
    _pass = false

  be complete(success: Bool) =>
    """
    MUST be called by each long test to indicate the test has finished.

    The "success" parameter specifies whether the test succeeded. However if
    the test has already been flagged as failing, then the test is considered a
    failure, regardless of the value of this parameter.

    No logging should be performed after this is called. Any that is will be
    ignored.
    """
    try
      let timer = _timer as Timer tag
      _timers.cancel(timer)
      _timer = None
    end

    if _completed then
      // We've already completed once, do nothing
      return
    end

    _completed = true

    if not success then
      _pass = false
    end

    // First tell the ponytest that we've completed, then our group.
    // When we tell the group another test may be started. If we did that first
    // then the ponytest might report the start of that new test before the end
    // of this one, which would make it look like exclusion wasn't working.
    let complete_log = _test_log = recover Array[String] end
    _ponytest._test_complete(_id, _pass, consume complete_log)

    _group._test_complete(this)
    
  be _timeout() =>
    """
    Called when the long test timeout expires.
    """
    if not _completed then
      log("Test timed out without completing", false)
      _test.timedout(_helper)
      complete(false)
    end
