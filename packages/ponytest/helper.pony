class TestHelper val
  """
  Publicly visible helper class that provides logging and assertion functions
  to unit tests.

  Each unit test is given a TestHelper when it is run. This is val and so can
  be passed between methods and actors within the test without restriction.

  The assertion functions throw an error if the condition fails. This can be
  propogated back to the top level test function, however it does not have to
  be. Once one assertion fails the test will be marked as a failure, regardless
  of what it does afterwards. It is perfectly acceptable to catch an error from
  one assertion failure, continue with the test and failure further asserts.
  All failed assertions will be reported.

  The expect functions are provided to make this approach easier. Each behaves
  exactly the same as the equivalent assert function, but does throw an error.

  All assert and expect functions take an optional message argument. This is
  simply a string that is printed as part of the error message when the
  condition fails. It is intended to aid identifying what failed.
  """

  let _tester: _Tester tag  // The tester we report everything to

  new _create(tester: _Tester tag) =>
    _tester = tester
    
  fun log(msg: String) =>
    """
    Log the given message.
    
    Test logs are usually only printed for tests that fail. The --log command
    line options overrides this and displays logs for all tests.

    Logs are printed one test at a time to avoid interleaving log lines from
    concurrent tests.
    """
    _tester._log(msg)

  fun assert_true(actual: Bool, msg: String = "Error") ? =>
    """
    Assert that the given expression is true.
    """
    if not actual then
      _tester._assert_failed("Assert true failed: " + msg)
      error
    end

  fun expect_true(actual: Bool, msg: String = "Error"): Bool =>
    """
    Expect that the given expression is true.
    """
    if not actual then
      _tester._assert_failed("Expect true failed: " + msg)
      return false
    end
    true

  fun assert_false(actual: Bool, msg: String = "Error") ? =>
    """
    Assert that the given expression is false.
    """
    if actual then
      _tester._assert_failed("Assert false failed: " + msg)
      error
    end

  fun expect_false(actual: Bool, msg: String = "Error"): Bool =>
    """
    Expect that the given expression is false.
    """
    if actual then
      _tester._assert_failed("Expect false failed: " + msg)
      return false
    end
    true

  fun assert_eq[A: (Comparable[A] box & Stringable)]
    (expect: A, actual: A, msg: String = "Error") ?
  =>
    """
    Assert that the 2 given expressions are equal.
    """
    if expect != actual then
      _tester._assert_failed("Assert EQ failed: " + msg +
        ". Expected (" + expect.string() + ") == (" +
        actual.string() + ")")
      error
    end

  fun expect_eq[A: (Comparable[A] box & Stringable)]
    (expect: A, actual: A, msg: String = "Error"): Bool
  =>
    """
    Expect that the 2 given expressions are equal.
    """
    if expect != actual then
      _tester._assert_failed("Expect EQ failed: " + msg +
        ". Expected (" + expect.string() + ") == (" +
        actual.string() + ")")
      return false
    end
    true

  fun complete(success: Bool) =>
    """
    MUST be called by each long test to indicate the test has finished.

    The "success" parameter specifies whether the test succeeded. However if
    any asserts fail the test will be considered a failure, regardless of the
    value of this parameter.
    """
    _tester._test_complete(success)
