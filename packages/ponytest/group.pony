trait _Group tag
  """
  Test exclusion is achieved by organising tests into groups. Each group can be
  exclusive, ie only one test is run at a time, or simultaneous, ie all tests
  are run concurrently.
  """

  be apply(test: TestHelper)
    """
    Run the given test, or queue it and run later, as appropriate.
    """

  be _test_complete(test: TestHelper)
    """
    The specified test has completed.
    """
    
actor _ExclusiveGroup is _Group
  """
  Test group in which we only ever have one test running at a time.
  """

  let _tests: Array[TestHelper] = Array[TestHelper]
  var _next: U64 = 0
  var _in_test:Bool = false

  be apply(test: TestHelper) =>
    if _in_test then
      // We're already running one test, save this one for later
      _tests.push(test)
    else
      // Run test now
      _in_test = true
      test._run()
    end

  be _test_complete(test: TestHelper) =>
    _in_test = false

    if _next < _tests.size() then
      // We have queued tests, run the next one
      try
        let next_test = _tests(_next)
        _next = _next + 1
        _in_test = true
        next_test._run()
      end
    end

    
actor _SimultaneousGroup is _Group
  """
  Test group in which all tests can run concurrently.
  """

  be apply(test: TestHelper) =>
    // Just run the test
    test._run()

  be _test_complete(test: TestHelper) =>
    // We don't care about tests finishing
    None
