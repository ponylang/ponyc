interface ITest
  """
  Interface for a test that can be run.
  """
  fun apply() ?
    """
    Runs the test.
    """

class val TestHelper
  """
  Per unit test class that provides control, logging and assertion functions.

  Each unit test is given a TestHelper when it is run. This is val and so can
  be passed between methods and actors within the test without restriction.

  The assertion functions check the relevant condition and mark the test as a
  failure if appropriate. The success or failure of the condition is reported
  back as a Bool which can be checked if a different code path is needed when
  that condition fails.

  All assert functions take an optional message argument. This is simply a
  string that is printed as part of the error message when the condition fails.
  It is intended to aid identifying what failed.
  """

  let _runner: _TestRunner
  let _sample_id: USize
  let env: Env
    """
    The process environment.

    This is useful for getting the [root authority](builtin-AmbientAuth.md) in
    order to access the filesystem (See [files](files--index.md)) or the network
    (See [net](net--index.md)) in your tests.
    """

  new val _create(runner: _TestRunner, env': Env) =>
    env = env'
    _runner = runner
    _sample_id = USize.max_value()

  new val _create_sample(
    runner: _TestRunner,
    env': Env,
    sample_id': USize)
  =>
    env = env'
    _runner = runner
    _sample_id = sample_id'

  fun log(msg: String, verbose: Bool = false) =>
    """
    Log the given message.

    The verbose parameter allows messages to be printed only when the --verbose
    command line option is used. For example, by default assert failures are
    logged, but passes are not. With --verbose both passes and fails are
    reported.

    Logs are printed one test at a time to avoid interleaving log lines from
    concurrent tests.
    """
    _runner.log(msg, verbose)

  fun fail(msg: String = "Test failed") =>
    """
    Flag the test as having failed.
    """
    _runner.fail(msg)

  fun assert_true(actual: Bool, msg: String = "", loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the given expression is true.
    """
    if not actual then
      fail(_format_loc(loc) + "Assert true failed. " + msg)
      return false
    end
    log(_format_loc(loc) + "Assert true passed. " + msg, true)
    true

  fun assert_false(actual: Bool, msg: String = "", loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the given expression is false.
    """
    if actual then
      fail(_format_loc(loc) + "Assert false failed. " + msg)
      return false
    end
    log(_format_loc(loc) + "Assert false passed. " + msg, true)
    true

  fun assert_error(test: ITest box, msg: String = "", loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the given test function throws an error when run.
    """
    try
      test()?
      fail(_format_loc(loc) + "Assert error failed. " + msg)
      false
    else
      log(_format_loc(loc) + "Assert error passed. " + msg, true)
      true
    end

  fun assert_no_error(
    test: ITest box,
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the given test function does not throw an error when run.
    """
    try
      test()?
      log(_format_loc(loc) + "Assert no error passed. " + msg, true)
      true
    else
      fail(_format_loc(loc) + "Assert no error failed. " + msg)
      true
    end

  fun assert_is[A](
    expect: A,
    actual: A,
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the 2 given expressions resolve to the same instance
    """
    _check_is[A]("is", consume expect, consume actual, msg, loc)

  fun _check_is[A](
    check: String,
    expect: A,
    actual: A,
    msg: String,
    loc: SourceLoc)
    : Bool
  =>
    """
    Check that the 2 given expressions resolve to the same instance
    """
    if expect isnt actual then
      fail(_format_loc(loc) + "Assert " + check + " failed. " + msg +
        " Expected (" + (digestof expect).string() + ") is (" +
        (digestof actual).string() + ")")
      return false
    end

    log(
      _format_loc(loc) + "Assert " + check + " passed. " + msg +
        " Got (" + (digestof expect).string() + ") is (" +
        (digestof actual).string() + ")",
      true)
    true

  fun assert_eq[A: (Equatable[A] #read & Stringable #read)]
    (expect: A, actual: A, msg: String = "", loc: SourceLoc = __loc): Bool
  =>
    """
    Assert that the 2 given expressions are equal.
    """
    _check_eq[A]("eq", expect, actual, msg, loc)

  fun _check_eq[A: (Equatable[A] #read & Stringable #read)]
    (check: String, expect: A, actual: A, msg: String, loc: SourceLoc)
    : Bool
  =>
    """
    Check that the 2 given expressions are equal.
    """
    if expect != actual then
      fail(_format_loc(loc) + "Assert " + check + " failed. " + msg +
        " Expected (" + expect.string() + ") == (" + actual.string() + ")")
      return false
    end

    log(_format_loc(loc) + "Assert " + check + " passed. " + msg +
      " Got (" + expect.string() + ") == (" + actual.string() + ")", true)
    true

  fun assert_isnt[A](
    not_expect: A,
    actual: A,
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the 2 given expressions resolve to different instances.
    """
    _check_isnt[A]("isn't", consume not_expect, consume actual, msg, loc)

  fun _check_isnt[A](
    check: String,
    not_expect: A,
    actual: A,
    msg: String,
    loc: SourceLoc)
    : Bool
  =>
    """
    Check that the 2 given expressions resolve to different instances.
    """
    if not_expect is actual then
      fail(_format_loc(loc) + "Assert " + check + " failed. " + msg +
        " Expected (" + (digestof not_expect).string() + ") isnt (" +
        (digestof actual).string() + ")")
      return false
    end

    log(
      _format_loc(loc) + "Assert " + check + " passed. " + msg +
        " Got (" + (digestof not_expect).string() + ") isnt (" +
        (digestof actual).string() + ")",
      true)
    true

  fun assert_ne[A: (Equatable[A] #read & Stringable #read)]
    (not_expect: A, actual: A, msg: String = "", loc: SourceLoc = __loc): Bool
  =>
    """
    Assert that the 2 given expressions are not equal.
    """
    _check_ne[A]("ne", not_expect, actual, msg, loc)

  fun _check_ne[A: (Equatable[A] #read & Stringable #read)]
    (check: String, not_expect: A, actual: A, msg: String, loc: SourceLoc)
    : Bool
  =>
    """
    Check that the 2 given expressions are not equal.
    """
    if not_expect == actual then
      fail(_format_loc(loc) + "Assert " + check + " failed. " + msg +
        " Expected (" + not_expect.string() + ") != (" + actual.string() +
        ")")
      return false
    end

    log(
      _format_loc(loc) + "Assert " + check + " passed. " + msg +
        " Got (" + not_expect.string() + ") != (" + actual.string() + ")",
      true)
    true

  fun assert_array_eq[A: (Equatable[A] #read & Stringable #read)](
    expect: ReadSeq[A],
    actual: ReadSeq[A],
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the contents of the 2 given ReadSeqs are equal.

    The type parameter of this function is the type parameter of the
    elements in both ReadSeqs. For instance, when comparing two `Array[U8]`,
    you should call this method as follows:

    ```pony
    fun apply(h: TestHelper) =>
      let a: Array[U8] = [1; 2; 3]
      let b: Array[U8] = [1; 2; 3]
      h.assert_array_eq[U8](a, b)
    ```
    """
    var ok = true

    if expect.size() != actual.size() then
      ok = false
    else
      try
        var i: USize = 0
        while i < expect.size() do
          if expect(i)? != actual(i)? then
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
      fail(_format_loc(loc) + "Assert EQ failed. " + msg + " Expected (" +
        _print_array[A](expect) + ") == (" + _print_array[A](actual) + ")")
      return false
    end

    log(
      _format_loc(loc) + "Assert EQ passed. " + msg + " Got (" +
        _print_array[A](expect) + ") == (" + _print_array[A](actual) + ")",
      true)
    true

  fun assert_array_eq_unordered[A: (Equatable[A] #read & Stringable #read)](
    expect: ReadSeq[A],
    actual: ReadSeq[A],
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the contents of the 2 given ReadSeqs are equal ignoring order.

    The type parameter of this function is the type parameter of the
    elements in both ReadSeqs. For instance, when comparing two `Array[U8]`,
    you should call this method as follows:

    ```pony
    fun apply(h: TestHelper) =>
      let a: Array[U8] = [1; 2; 3]
      let b: Array[U8] = [1; 3; 2]
      h.assert_array_eq_unordered[U8](a, b)
    ```
    """
    try
      let missing = Array[box->A]
      let consumed = Array[Bool].init(false, actual.size())
      for e in expect.values() do
        var found = false
        var i: USize = -1
        for a in actual.values() do
          i = i + 1
          if consumed(i)? then continue end
          if e == a then
            consumed.update(i, true)?
            found = true
            break
          end
        end
        if not found then
          missing.push(e)
        end
      end

      let extra = Array[box->A]
      for (i, c) in consumed.pairs() do
        if not c then extra.push(actual(i)?) end
      end

      if (extra.size() != 0) or (missing.size() != 0) then
        fail(
          _format_loc(loc) + "Assert EQ_UNORDERED failed. " + msg +
            " Expected (" + _print_array[A](expect) + ") == (" +
            _print_array[A](actual) + "):" +
            "\nMissing: " + _print_array[box->A](missing) +
            "\nExtra: " + _print_array[box->A](extra))
        return false
      end
      log(
        _format_loc(loc) + "Assert EQ_UNORDERED passed. " + msg + " Got (" +
          _print_array[A](expect) + ") == (" + _print_array[A](actual) + ")",
        true)
      true
    else
      fail("Assert EQ_UNORDERED failed from an internal error.")
      false
    end

  fun _format_loc(loc: SourceLoc): String =>
    loc.file() + ":" + loc.line().string() + ": "

  fun _print_array[A: Stringable #read](array: ReadSeq[A]): String =>
    """
    Generate a printable string of the contents of the given readseq to use in
    error messages.

    The type parameter of this function is the type parameter of the
    elements in the ReadSeq.
    """
    "[len=" + array.size().string() + ": " + ", ".join(array.values()) + "]"

  fun long_test(timeout: U64) =>
    """
    Indicate that this is a long running test that may continue after the
    test function exits.
    Once this function is called, complete() must be called to finish the test,
    unless a timeout occurs.
    The timeout is specified in nanseconds.
    """
    _runner.long_test(timeout)

  fun complete(success: Bool) =>
    """
    MUST be called by each long test to indicate the test has finished, unless
    a timeout occurs. If you are using expect_action() then complete(true) will
    be called once the last expected action has been completed via
    complete_action().

    The "success" parameter specifies whether the test succeeded. However if
    any asserts fail the test will be considered a failure, regardless of the
    value of this parameter.

    Once this is called tear_down() may be called at any time.
    """
    _runner.complete(success, _sample_id)

  fun expect_action(name: String) =>
    """
    Can be called in a long test to set up expectations for one or more actions
    that, when all completed, will complete the test.

    This pattern is useful for cases where you have multiple things that need
    to happen to complete your test, but don't want to have to collect them
    all yourself into a single actor that calls the complete method.

    The order of calls to expect_action don't matter - the actions may be
    completed in any other order to complete the test.
    """
    _runner.expect_action(name, _sample_id)

  fun complete_action(name: String) =>
    """
    MUST be called for each action expectation that was set up in a long test
    to fulfill the expectations. Any expectations that are still outstanding
    when the long test timeout runs out will be printed by name when it fails.

    Completing all outstanding actions is enough to finish the test. There's no
    need to also call the complete method when the actions are finished.

    Calling the complete method will finish the test immediately, without
    waiting for any outstanding actions to be completed.
    """
    _runner.complete_action(name, true, _sample_id)

  fun fail_action(name: String) =>
    """
    Call to fail an action, which will also cause the entire test to fail
    immediately, without waiting the rest of the outstanding actions.

    The name of the failed action will be included in the failure output.

    Usually the action name should be an expected action set up by a call to
    expect_action, but failing unexpected actions will also fail the test.
    """
    _runner.complete_action(name, false, _sample_id)

  fun dispose_when_done(disposable: DisposableActor) =>
    """
    Pass a disposable actor to be disposed of when the test is complete.
    The actor will be disposed no matter whether the test succeeds or fails.

    If the test is already tearing down, the actor will be disposed immediately.
    """
    _runner.dispose_when_done(disposable, _sample_id)

  fun classify(label: String) =>
    """
    Classify the current property sample with the given label.
    """
    _runner._property_classify(label, _sample_id)

  fun collect(value: Stringable) =>
    """
    Classify the current property sample using the string representation
    of `value`.
    """
    classify(value.string())

  fun tabulate(heading: String, label: String) =>
    """
    Tabulate the current property sample under `heading` with the given
    `label`.
    """
    _runner._property_tabulate(heading, label, _sample_id)

  fun cover(condition: Bool, label: String, min_pct: F64 = 0.0) =>
    """
    Check that `condition` holds for at least `min_pct` percent of samples.
    """
    _runner._property_cover(condition, label, min_pct, _sample_id)

  fun val for_all[T](gen: Generator[T] val): ForAll[T] =>
    """
    Run an inline property check with one generated argument.
    """
    ForAll[T](gen, this)

  fun val for_all2[T1, T2](
    gen1: Generator[T1] val,
    gen2: Generator[T2] val)
    : ForAll2[T1, T2]
  =>
    """
    Run an inline property check with two generated arguments.
    """
    ForAll2[T1, T2](gen1, gen2, this)

  fun val for_all3[T1, T2, T3](
    gen1: Generator[T1] val,
    gen2: Generator[T2] val,
    gen3: Generator[T3] val)
    : ForAll3[T1, T2, T3]
  =>
    """
    Run an inline property check with three generated arguments.
    """
    ForAll3[T1, T2, T3](gen1, gen2, gen3, this)

  fun val for_all4[T1, T2, T3, T4](
    gen1: Generator[T1] val,
    gen2: Generator[T2] val,
    gen3: Generator[T3] val,
    gen4: Generator[T4] val)
    : ForAll4[T1, T2, T3, T4]
  =>
    """
    Run an inline property check with four generated arguments.
    """
    ForAll4[T1, T2, T3, T4](gen1, gen2, gen3, gen4, this)

  fun _start_property(exec: _PropertyExecution iso) =>
    """
    Enter property mode. Called from PropertyTest/StatefulPropertyTest
    wrappers.
    """
    _runner._start_property(consume exec)
