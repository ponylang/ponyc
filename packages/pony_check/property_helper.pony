
interface val _PropertyRunNotify
  """
  Simple callback for notifying the runner
  that a run completed.
  """
  fun apply(round: _Round, success: Bool)

interface tag _IPropertyRunner
  """
  Interface for a PropertyRunner without the generic type parameter,
  and only with the behaviours we are interested in.
  """

  be expect_action(name: String, round: _Round)

  be complete_action(name: String, round: _Round, ph: PropertyHelper)

  be fail_action(name: String, round: _Round, ph: PropertyHelper)

  be dispose_when_done(disposable: DisposableActor, round: _Round)

  be log(msg: String, verbose: Bool = false)


class val PropertyHelper
  """
  Helper for PonyCheck properties.

  Mirrors the [TestHelper](pony_test-TestHelper.md) API as closely as possible.

  Contains assertion functions and functions for completing asynchronous
  properties, for expecting and completing or failing actions.

  Internally a new PropertyHelper will be created for each call to
  a property with a new sample and also for every shrink run.
  So don't assume anything about the identity of the PropertyHelper inside of
  your Properties.

  This class is `val` by default so it can be safely passed around to other
  actors.

  It exposes the process [Env](builtin-Env.md) as public `env` field in order to
  give access to the root authority and other stuff.
  """
  let _runner: _IPropertyRunner
  let _run_notify: _PropertyRunNotify
  let _run: _Round
  let _params: String

  let env: Env

  new val create(
    env': Env,
    runner: _IPropertyRunner,
    run_notify: _PropertyRunNotify,
    run: _Round,
    params: String
  ) =>
    env = env'
    _runner = runner
    _run_notify = run_notify
    _run = run
    _params = params

/****** START DUPLICATION FROM TESTHELPER ********/

  fun log(msg: String, verbose: Bool = false) =>
    """
    Log the given message.

    The verbose parameter allows messages to be printed only when the
    `--verbose` command line option is used. For example, by default assert
    failures are logged, but passes are not. With `--verbose`, both passes and
    fails are reported.

    Logs are printed one test at a time to avoid interleaving log lines from
    concurrent tests.
    """
    _runner.log(msg, verbose)

  fun fail(msg: String = "Test failed") =>
    """
    Flag the test as having failed.
    """
    _fail(msg)

  fun assert_false(
    predicate: Bool,
    msg: String val = "",
    loc: SourceLoc val = __loc)
    : Bool val
  =>
    """
    Assert that the given expression is false.
    """
    if predicate then
      _fail(_fmt_msg(loc, "Assert false failed. " + msg))
      return false
    end
    _runner.log(_fmt_msg(loc, "Assert false passed. " + msg))
    true

  fun assert_true(
    predicate: Bool,
    msg: String val = "",
    loc: SourceLoc val = __loc)
    : Bool val
  =>
    """
    Assert that the given expression is true.
    """
    if not predicate then
      _fail(_fmt_msg(loc, "Assert true failed. " + msg))
      return false
    end
    _runner.log(_fmt_msg(loc, "Assert true passed. " + msg))
    true

  fun assert_error(
    test: {(): None ?} box,
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the given test function throws an error when run.
    """
    try
      test()?
      _fail(_fmt_msg(loc, "Assert error failed. " + msg))
      false
    else
      _runner.log(_fmt_msg(loc, "Assert error passed. " + msg), true)
      true
    end

  fun assert_no_error(
    test: {(): None ?} box,
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the given test function does not throw an error when run.
    """
    try
      test()?
      _runner.log(_fmt_msg(loc, "Assert no error passed. " + msg), true)
      true
    else
      _fail(_fmt_msg(loc, "Assert no error failed. " + msg))
      false
    end

  fun assert_is[A](
    expect: A,
    actual: A,
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the 2 given expressions resolve to the same instance.
    """
    if expect isnt actual then
      _fail(_fmt_msg(loc, "Assert is failed. " + msg
        + " Expected (" + (digestof expect).string() + ") is ("
        + (digestof actual).string() + ")"))
      return false
    end

    _runner.log(
      _fmt_msg(loc, "Assert is passed. " + msg
        + " Got (" + (digestof expect).string() + ") is ("
        + (digestof actual).string() + ")"),
      true)
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
    if not_expect is actual then
      _fail(_fmt_msg(loc, "Assert isn't failed. " + msg
        + " Expected (" + (digestof not_expect).string() + ") isnt ("
        + (digestof actual).string() + ")"))
      return false
    end

    _runner.log(
      _fmt_msg(loc, "Assert isn't passed. " + msg
        + " Got (" + (digestof not_expect).string() + ") isnt ("
        + (digestof actual).string() + ")"),
      true)
    true

  fun assert_eq[A: (Equatable[A] #read & Stringable #read)](
    expect: A,
    actual: A,
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the 2 given expressions are equal.
    """
    if expect != actual then
      _fail(_fmt_msg(loc,  "Assert eq failed. " + msg
        + " Expected (" + expect.string() + ") == (" + actual.string() + ")"))
      return false
    end

    _runner.log(_fmt_msg(loc, "Assert eq passed. " + msg
      + " Got (" + expect.string() + ") == (" + actual.string() + ")"),
      true)
    true

  fun assert_ne[A: (Equatable[A] #read & Stringable #read)](
    not_expect: A,
    actual: A,
    msg: String = "",
    loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that the 2 given expressions are not equal.
    """
    if not_expect == actual then
      _fail(_fmt_msg(loc, "Assert ne failed. " + msg
        + " Expected (" + not_expect.string() + ") != (" + actual.string()
        + ")"))
      return false
    end

    _runner.log(
      _fmt_msg(loc, "Assert ne passed. " + msg
        + " Got (" + not_expect.string() + ") != (" + actual.string() + ")"),
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
      _fail(_fmt_msg(loc, "Assert EQ failed. " + msg + " Expected ("
        + _print_array[A](expect) + ") == (" + _print_array[A](actual) + ")"))
      return false
    end

    _runner.log(
      _fmt_msg(loc, "Assert EQ passed. " + msg + " Got ("
        + _print_array[A](expect) + ") == (" + _print_array[A](actual) + ")"),
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
        _fail(
          _fmt_msg(loc, "Assert EQ_UNORDERED failed. " + msg
            + " Expected (" + _print_array[A](expect) + ") == ("
            + _print_array[A](actual) + "):"
            + "\nMissing: " + _print_array[box->A](missing)
            + "\nExtra: " + _print_array[box->A](extra)
          )
        )
        return false
      end
      _runner.log(
        _fmt_msg(
          loc,
          "Assert EQ_UNORDERED passed. "
          + msg
          + " Got ("
          + _print_array[A](expect)
          + ") == ("
          + _print_array[A](actual)
          + ")"
        ),
        true
      )
      true
    else
      _fail("Assert EQ_UNORDERED failed from an internal error.")
      false
    end

  fun _print_array[A: Stringable #read](array: ReadSeq[A]): String =>
    """
    Generate a printable string of the contents of the given readseq to use in
    error messages.
    """
    "[len=" + array.size().string() + ": " + ", ".join(array.values()) + "]"


/****** END DUPLICATION FROM TESTHELPER *********/

  fun expect_action(name: String) =>
    """
    Expect some action of the given name to complete
    for the property to hold.

    If all expected actions are completed successfully,
    the property is considered successful.

    If 1 action fails, the property is considered failing.

    Call `complete_action(name)` or `fail_action(name)`
    to mark some action as completed.

    Example:

    ```pony
      actor AsyncActor

        let _ph: PropertyHelper

        new create(ph: PropertyHelper) =>
          _ph = ph

        be complete(s: String) =>
          if (s.size() % 2) == 0 then
            _ph.complete_action("is_even")
          else
            _ph.fail_action("is_even")

      class EvenStringProperty is Property1[String]
        fun name(): String => "even_string"

        fun gen(): Generator[String] =>
          Generators.ascii()

      fun property(arg1: String, ph: PropertyHelper) =>
        ph.expect_action("is_even")
        AsyncActor(ph).check(arg1)
    ```

    """
    _runner.expect_action(name, _run)

  fun val complete_action(name: String) =>
    """
    Complete an expected action successfully.

    If all expected actions are completed successfully,
    the property is considered successful.

    If 1 action fails, the property is considered failing.

    If the action `name` was not expected, i.e. was not registered using
    `expect_action`, nothing happens.
    """
    _runner.complete_action(name, _run, this)

  fun val fail_action(name: String) =>
    """
    Mark an expected action as failed.

    If all expected actions are completed successfully,
    the property is considered successful.

    If 1 action fails, the property is considered failing.
    """
    _runner.fail_action(name, _run, this)

  fun complete(success: Bool) =>
    """
    Complete an asynchronous property successfully.

    Once this method is called the property
    is considered successful or failing
    depending on the value of the parameter `success`.

    For more fine grained control over completing or failing
    a property that consists of many steps, consider using
    `expect_action`, `complete_action` and `fail_action`.
    """
    _run_notify.apply(_run, success)

  fun dispose_when_done(disposable: DisposableActor) =>
    """
    Dispose the actor after a property run / a shrink is done.
    """
    _runner.dispose_when_done(disposable, _run)

  fun _fail(msg: String) =>
    _runner.log(msg)
    _run_notify.apply(_run, false)

  fun _fmt_msg(loc: SourceLoc, msg: String): String =>
    let msg_prefix = _params + " " + _run.string() + " " + _format_loc(loc)
    if msg.size() > 0 then
      msg_prefix + ": " + msg
    else
      msg_prefix
    end

  fun _format_loc(loc: SourceLoc): String =>
    loc.file() + ":" + loc.line().string()


