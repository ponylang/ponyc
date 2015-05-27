actor TestHelper
  """
  Per unit test actor that runs the test and provides logging and assertion
  functions to it.

  Each unit test is given a TestHelper when it is run. This is tag and so can
  be passed between methods and actors within the test without restriction.

  The assertion functions throw an error if the condition fails. This can be
  propogated back to the top level test function, however it does not have to
  be. Once one assertion fails the test will be marked as a failure, regardless
  of what it does afterwards. It is perfectly acceptable to catch an error from
  one assertion failure, continue with the test and failure further asserts.
  All failed assertions will be reported.

  The expect functions are provided to make this approach easier. Each behaves
  exactly the same as the equivalent assert function, but does not throw an
  error.

  All assert and expect functions take an optional message argument. This is
  simply a string that is printed as part of the error message when the
  condition fails. It is intended to aid identifying what failed.
  """

  let _ponytest: PonyTest
  let _id: U64
  let _group: _Group
  let _test: UnitTest iso
  var _test_log: Array[String] iso = recover Array[String] end
  var _pass: Bool = false
  var _completed: Bool = false

  new _create(ponytest: PonyTest, id: U64, test: UnitTest iso, group: _Group)
  =>
    """
    Create a new TestHelper.
    ponytest - The authority we report everything to.
    id - Test identifier needed when reporting to ponytest.
    test - The test to run and help.
    group - The group this test is in, which must be notified when we finish.
    """
    _ponytest = ponytest
    _id = id
    _test = consume test
    _group = group

  be _run() =>
    """
    Run our test.
    """
    _pass = true
    var pass: TestResult = false
    _ponytest._test_started(_id)

    try
      // If the test throws then pass will keep its value of false, no extra
      // processing is needed.
      pass = _test(recover tag this end)
    end

    match pass
    | var p: Bool =>
      // This isn't a long test, ie the test is complete.
      // We need to process any pending log and failure messages. Send
      // ourselves a complete() message so we know when we're done.
      complete(p)
    end
    
  be log(msg: String) =>
    """
    Log the given message.
    
    Test logs are usually only printed for tests that fail. The --log command
    line options overrides this and displays logs for all tests.

    Logs are printed one test at a time to avoid interleaving log lines from
    concurrent tests.
    """
    _test_log.push(msg)

  be fail() =>
    """
    Flag the test as having failed.
    Note that this does not add anything to the log, which can make it
    difficult to debug the test as the cause of the failure is not obvious. If
    you do call this directly consider also writing a log message.
    In general it is better to use the assert_* and expect_* functions.
    """
    _pass = false

  fun tag assert_true(actual: Bool, msg: String = "Error") ? =>
    """
    Assert that the given expression is true.
    """
    if not actual then
      _assert_failed("Assert true failed: " + msg)
      error
    end

  fun tag expect_true(actual: Bool, msg: String = "Error"): Bool =>
    """
    Expect that the given expression is true.
    """
    if not actual then
      _assert_failed("Expect true failed: " + msg)
      return false
    end
    true

  fun tag assert_false(actual: Bool, msg: String = "Error") ? =>
    """
    Assert that the given expression is false.
    """
    if actual then
      _assert_failed("Assert false failed: " + msg)
      error
    end

  fun tag expect_false(actual: Bool, msg: String = "Error"): Bool =>
    """
    Expect that the given expression is false.
    """
    if actual then
      _assert_failed("Expect false failed: " + msg)
      return false
    end
    true

  fun tag assert_eq[A: (Comparable[A] box & Stringable)]
    (expect: A, actual: A, msg: String = "Error") ?
  =>
    """
    Assert that the 2 given expressions are equal.
    """
    if not _check_eq[A]("Assert", expect, actual, msg) then
      error
    end

  fun tag expect_eq[A: (Comparable[A] box & Stringable)]
    (expect: A, actual: A, msg: String = "Error"): Bool
  =>
    """
    Expect that the 2 given expressions are equal.
    """
    _check_eq[A]("Expect", expect, actual, msg)

  fun tag _check_eq[A: (Comparable[A] box & Stringable)]
    (verb: String, expect: A, actual: A, msg: String): Bool
  =>
    """
    Check that the 2 given expressions are equal.
    """
    if expect != actual then
      _assert_failed(verb + " EQ failed: " + msg +
        ". Expected (" + expect.string() + ") == (" +
        actual.string() + ")")
      return false
    end
    true
    
  be _assert_failed(msg: String) =>
    """
    Record that an assert failed and log the given message
    """
    _test_log.push(msg)
    _pass = false

  be complete(success: Bool) =>
    """
    MUST be called by each long test to indicate the test has finished.

    The "success" parameter specifies whether the test succeeded. However if
    any asserts fail the test will be considered a failure, regardless of the
    value of this parameter.

    No logging or asserting should be performed after this is called. Any that
    is will be ignored.
    """
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
