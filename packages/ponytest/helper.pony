interface ITest
  fun apply()?

class val TestHelper
  """
  Per unit test class that provides control, logging and assertion functions.

  Each unit test is given a TestHelper when it is run. This is val and so can
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

  let _runner: _TestRunner
  let env: Env

  new val _create(runner: _TestRunner, env': Env)
  =>
    """
    Create a new TestHelper.
    """
    env = env'
    _runner = runner

  fun log(msg: String, verbose: Bool = false) =>
    """
    Log the given message.

    The verbose parameter allows messages to be printed only when the --verbose
    command line option is used. For example, by default assert and expect
    failures are logged, but passes are not. With --verbose both passes and
    fails are reported.

    Logs are printed one test at a time to avoid interleaving log lines from
    concurrent tests.
    """
    _runner.log(msg, verbose)

  fun fail() =>
    """
    Flag the test as having failed.
    Note that this does not add anything to the log, which can make it
    difficult to debug the test as the cause of the failure is not obvious. If
    you do call this directly consider also writing a log message.
    In general it is better to use the assert_* and expect_* functions.
    """
    _runner.fail()

  fun assert_failed(msg: String) =>
    """
    Assert failure of the test.
    Record that an assert failed and log the given message.
    """
    _runner.log(msg, false)
    _runner.fail()

  fun assert_true(actual: Bool, msg: String = "") ? =>
    """
    Assert that the given expression is true.
    """
    if not actual then
      assert_failed("Assert true failed. " + msg)
      error
    end
    log("Assert true passed. " + msg, true)

  fun expect_true(actual: Bool, msg: String = ""): Bool =>
    """
    Expect that the given expression is true.
    """
    if not actual then
      assert_failed("Expect true failed. " + msg)
      return false
    end
    log("Expect true passed. " + msg, true)
    true

  fun assert_false(actual: Bool, msg: String = "") ? =>
    """
    Assert that the given expression is false.
    """
    if actual then
      assert_failed("Assert false failed. " + msg)
      error
    end
    log("Assert false passed. " + msg, true)

  fun expect_false(actual: Bool, msg: String = ""): Bool =>
    """
    Expect that the given expression is false.
    """
    if actual then
      assert_failed("Expect false failed. " + msg)
      return false
    end
    log("Expect false passed. " + msg, true)
    true

  fun assert_error(test: ITest box, msg: String = "") ? =>
    """
    Assert that the given test function throws an error when run.
    """
    try
      test()
      assert_failed("Assert error failed. " + msg)
    else
      log("Assert error passed. " + msg, true)
      return
    end
    error

  fun expect_error(test: ITest box, msg: String = ""): Bool =>
    """
    Expect that the given test function throws an error when run.
    """
    try
      test()
      assert_failed("Expect error failed. " + msg)
      false
    else
      log("Expect error passed. " + msg, true)
      true
    end

  fun assert_is[A](expect: A, actual: A, msg: String = "") ? =>
    """
    Assert that the 2 given expressions resolve to the same instance
    """
    let expect' = identityof expect
    let actual' = identityof actual
    if not _check_eq[U64]("Assert", expect', actual', msg) then
      error
    end

  fun expect_is[A](expect: A, actual: A, msg: String = ""): Bool =>
    """
    Expect that the 2 given expressions resolve to the same instance
    """
    let expect' = identityof expect
    let actual' = identityof actual
    _check_eq[U64]("Expect", expect', actual', msg)

  fun assert_eq[A: (Equatable[A] #read & Stringable #read)]
    (expect: A, actual: A, msg: String = "") ?
  =>
    """
    Assert that the 2 given expressions are equal.
    """
    if not _check_eq[A]("Assert", expect, actual, msg) then
      error
    end

  fun expect_eq[A: (Equatable[A] #read & Stringable #read)]
    (expect: A, actual: A, msg: String = ""): Bool
  =>
    """
    Expect that the 2 given expressions are equal.
    """
    _check_eq[A]("Expect", expect, actual, msg)

  fun _check_eq[A: (Equatable[A] #read & Stringable)]
    (verb: String, expect: A, actual: A, msg: String): Bool
  =>
    """
    Check that the 2 given expressions are equal.
    """
    if expect != actual then
      assert_failed(verb + " EQ failed. " + msg +
        " Expected (" + expect.string() + ") == (" + actual.string() + ")")
      return false
    end

    log(verb + " EQ passed. " + msg +
      " Got (" + expect.string() + ") == (" + actual.string() + ")", true)
    true

  fun assert_ne[A: (Equatable[A] #read & Stringable #read)]
    (not_expect: A, actual: A, msg: String = "") ?
  =>
    """
    Assert that the 2 given expressions are not equal.
    """
    if not _check_ne[A]("Assert", not_expect, actual, msg) then
      error
    end

  fun expect_ne[A: (Equatable[A] #read & Stringable #read)]
    (not_expect: A, actual: A, msg: String = ""): Bool
  =>
    """
    Expect that the 2 given expressions are not equal.
    """
    _check_ne[A]("Expect", not_expect, actual, msg)

  fun _check_ne[A: (Equatable[A] #read & Stringable)]
    (verb: String, not_expect: A, actual: A, msg: String): Bool
  =>
    """
    Check that the 2 given expressions are not equal.
    """
    if not_expect == actual then
      assert_failed(verb + " NE failed. " + msg +
        " Expected (" + not_expect.string() + ") != (" + actual.string() + ")")
      return false
    end

    log(verb + " NE passed. " + msg +
      " Got (" + not_expect.string() + ") != (" + actual.string() + ")", true)
    true

  fun assert_array_eq[A: (Equatable[A] #read & Stringable #read)]
    (expect: ReadSeq[A], actual: ReadSeq[A], msg: String = "") ?
  =>
    """
    Assert that the contents of the 2 given ReadSeqs are equal.
    """
    if not _check_array_eq[A]("Assert", expect, actual, msg) then
      error
    end

  fun expect_array_eq[A: (Equatable[A] #read & Stringable #read)]
    (expect: ReadSeq[A], actual: ReadSeq[A], msg: String = ""): Bool
  =>
    """
    Expect that the contents of the 2 given ReadSeqs are equal.
    """
    _check_array_eq[A]("Expect", expect, actual, msg)

  fun _check_array_eq[A: (Equatable[A] #read & Stringable #read)]
    (verb: String, expect: ReadSeq[A], actual: ReadSeq[A], msg: String): Bool
  =>
    """
    Check that the contents of the 2 given ReadSeqs are equal.
    """
    var ok = true

    if expect.size() != actual.size() then
      ok = false
    else
      try
        var i: USize = 0
        while i < expect.size() do
          if expect(i) != actual(i) then
            ok = false
            break
          end

          i = i + 1
        end
      else
        ok = false
      end
    end

    if not ok then
      assert_failed(verb + " EQ failed. " + msg + " Expected (" +
        _print_array[A](expect) + ") == (" + _print_array[A](actual) + ")")
      return false
    end

    log(verb + " EQ passed. " + msg + " Got (" +
      _print_array[A](expect) + ") == (" + _print_array[A](actual) + ")", true)
    true

  fun assert_array_eq_unordered[A: (Equatable[A] #read & Stringable #read)]
    (expect: ReadSeq[A], actual: ReadSeq[A], msg: String = "") ?
  =>
    """
    Assert that the contents of the 2 given ReadSeqs are equal ignoring order.
    """
    if not _check_array_eq_unordered[A]("Assert", expect, actual, msg) then
      error
    end

  fun expect_array_eq_unordered[A: (Equatable[A] #read & Stringable #read)]
    (expect: ReadSeq[A], actual: ReadSeq[A], msg: String = ""): Bool
  =>
    """
    Expect that the contents of the 2 given ReadSeqs are equal ignoring order.
    """
    _check_array_eq_unordered[A]("Expect", expect, actual, msg)

  fun _check_array_eq_unordered[A: (Equatable[A] #read & Stringable #read)]
    (verb: String, expect: ReadSeq[A], actual: ReadSeq[A], msg: String): Bool
  =>
    """
    Check that the contents of the 2 given ReadSeqs are equal.
    """
    try
      let missing = Array[A]
      let consumed = Array[Bool].init(false, actual.size())
      for e in expect.values() do
        var found = false
        var i: USize = -1
        for a in actual.values() do
          i = i + 1
          if consumed(i) then continue end
          if e == a then
            consumed.update(i, true)
            found = true
            break
          end
        end
        if not found then
          missing.push(e)
        end
      end

      let extra = Array[A]
      for (i, c) in consumed.pairs() do
        if not c then extra.push(actual(i)) end
      end

      if (extra.size() != 0) or (missing.size() != 0) then
        assert_failed(
          verb + " EQ_UNORDERED failed. " + msg + " Expected (" +
          _print_array[A](expect) + ") == (" + _print_array[A](actual) + "):" +
          "\nMissing: " + _print_array[A](missing) +
          "\nExtra: " + _print_array[A](extra))
        return false
      end
      log(
        verb + " EQ_UNORDERED passed. " + msg + " Got (" +
        _print_array[A](expect) + ") == (" + _print_array[A](actual) + ")",
        true)
      true
    else
      assert_failed(verb + " EQ_UNORDERED failed from an internal error.")
      false
    end


  fun _print_array[A: Stringable #read](array: ReadSeq[A]): String =>
    """
    Generate a printable string of the contents of the given readseq to use in
    error messages.
    """
    "[len=" + array.size().string() + ": " + ", ".join(array) + "]"

  fun complete(success: Bool) =>
    """
    MUST be called by each long test to indicate the test has finished.

    The "success" parameter specifies whether the test succeeded. However if
    any asserts fail the test will be considered a failure, regardless of the
    value of this parameter.

    No logging or asserting should be performed after this is called. Any that
    is will be ignored.
    """
    _runner.complete(success)
