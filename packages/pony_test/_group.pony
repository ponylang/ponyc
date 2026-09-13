use @pony_schedulers[U32]()

trait tag _Group
  """
  Test exclusion is achieved by organising tests into groups. Each group can be
  exclusive, ie only one test is run at a time, or simultaneous, ie tests run
  concurrently up to a limit.
  """

  be apply(runner: _TestRunner)
    """
    Run the given test, or queue it and run later, as appropriate.
    """

  be _test_complete(runner: _TestRunner)
    """
    The specified test has completed.
    """

actor _ExclusiveGroup is _Group
  """
  Test group in which we only ever have one test running at a time.
  """

  embed _tests: Array[_TestRunner] = Array[_TestRunner]
  var _next: USize = 0
  var _in_test:Bool = false

  be apply(runner: _TestRunner) =>
    if _in_test then
      // We're already running one test, save this one for later
      _tests.push(runner)
    else
      // Run test now
      _in_test = true
      runner.run()
    end

  be _test_complete(runner: _TestRunner) =>
    _in_test = false

    if _next < _tests.size() then
      // We have queued tests, run the next one
      try
        let next_test = _tests(_next)?
        _next = _next + 1
        _in_test = true
        next_test.run()
      end
    end

actor _SimultaneousGroup is _Group
  """
  Test group that runs tests concurrently up to the number of scheduler
  threads. Tests beyond the limit are queued and started as earlier tests
  complete.
  """

  embed _tests: Array[_TestRunner] = Array[_TestRunner]
  var _next: USize = 0
  var _running: U32 = 0
  let _max: U32

  new create() =>
    _max = @pony_schedulers()

  be apply(runner: _TestRunner) =>
    if _running < _max then
      _running = _running + 1
      runner.run()
    else
      _tests.push(runner)
    end

  be _test_complete(runner: _TestRunner) =>
    _running = _running - 1

    if _next < _tests.size() then
      try
        let next_test = _tests(_next)?
        _next = _next + 1
        _running = _running + 1
        next_test.run()
      end
    end
