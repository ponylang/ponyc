class val PropertyHelper
  """
  Per-sample helper for property-based tests.

  Each instance is scoped to a single sample. Classification calls
  (`classify`, `tabulate`, `cover`) count toward that sample, and
  `complete`/`expect_action`/`complete_action` complete the sample,
  not the overall test.
  """
  let _h: TestHelper
  let _sample_id: USize
  let env: Env

  new val _create(h: TestHelper, sample_id': USize) =>
    _h = h
    _sample_id = sample_id'
    env = h.env

  fun classify(label: String) =>
    """
    Tag this sample with `label` for distribution reporting.
    """
    _h._property_classify(label, _sample_id)

  fun collect(value: Stringable) =>
    """
    Classify this sample by the string representation of `value`.
    """
    classify(value.string())

  fun tabulate(heading: String, label: String) =>
    """
    Record `label` under `heading` for cross-tabulation reporting.
    """
    _h._property_tabulate(heading, label, _sample_id)

  fun cover(condition: Bool, label: String, min_pct: F64 = 0.0) =>
    """
    Record whether `condition` holds for `label`. The property fails
    if fewer than `min_pct` percent of samples satisfy the condition.
    The default `min_pct` of 0.0 reports coverage without enforcing it.
    """
    _h._property_cover(condition, label, min_pct, _sample_id)

  fun log(msg: String, verbose: Bool = false) =>
    """
    Log a message. When `verbose` is true, the message is only shown
    in verbose output mode.
    """
    _h.log(msg, verbose)

  fun fail(msg: String = "Test failed") =>
    """
    Flag this sample as having failed.
    """
    _h._fail_sample(msg, _sample_id)

  fun assert_true(actual: Bool, msg: String = "", loc: SourceLoc = __loc)
    : Bool
  =>
    """
    Assert that `actual` is true.
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
    Assert that `actual` is false.
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
    Assert that `test` throws an error when called.
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
    Assert that `test` does not throw an error when called.
    """
    try
      test()?
      log(_format_loc(loc) + "Assert no error passed. " + msg, true)
      true
    else
      fail(_format_loc(loc) + "Assert no error failed. " + msg)
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
    Assert that `expect` and `actual` are the same instance.
    """
    if expect isnt actual then
      fail(
        _format_loc(loc) + "Assert is failed. " + msg +
          " Expected (" + (digestof expect).string() + ") is (" +
          (digestof actual).string() + ")")
      return false
    end
    log(
      _format_loc(loc) + "Assert is passed. " + msg +
        " Got (" + (digestof expect).string() + ") is (" +
        (digestof actual).string() + ")",
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
    Assert that `not_expect` and `actual` are different instances.
    """
    if not_expect is actual then
      fail(
        _format_loc(loc) + "Assert isn't failed. " + msg +
          " Expected (" + (digestof not_expect).string() + ") isnt (" +
          (digestof actual).string() + ")")
      return false
    end
    log(
      _format_loc(loc) + "Assert isn't passed. " + msg +
        " Got (" + (digestof not_expect).string() + ") isnt (" +
        (digestof actual).string() + ")",
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
    Assert that `expect` and `actual` are equal.
    """
    if expect != actual then
      fail(
        _format_loc(loc) + "Assert eq failed. " + msg +
          " Expected (" + expect.string() + ") == (" +
          actual.string() + ")")
      return false
    end
    log(
      _format_loc(loc) + "Assert eq passed. " + msg +
        " Got (" + expect.string() + ") == (" + actual.string() + ")",
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
    Assert that `not_expect` and `actual` are not equal.
    """
    if not_expect == actual then
      fail(
        _format_loc(loc) + "Assert ne failed. " + msg +
          " Expected (" + not_expect.string() + ") != (" +
          actual.string() + ")")
      return false
    end
    log(
      _format_loc(loc) + "Assert ne passed. " + msg +
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
    Assert that the contents of `expect` and `actual` are equal.
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
      fail(
        _format_loc(loc) + "Assert EQ failed. " + msg + " Expected (" +
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
    Assert that the contents of `expect` and `actual` are equal
    regardless of order.
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

  fun long_test(timeout: U64) =>
    """
    Switch this property to async mode. Each sample runs until
    `complete` is called or `timeout` nanoseconds elapse.
    """
    _h.long_test(timeout)

  fun complete(success: Bool) =>
    """
    Complete this sample. Only needed for async properties
    (those that call `long_test`).
    """
    _h._complete_sample(success, _sample_id)

  fun expect_action(name: String) =>
    """
    Register an expected action for this sample. All expected
    actions must complete before the sample finishes.
    """
    _h._expect_action_sample(name, _sample_id)

  fun complete_action(name: String) =>
    """
    Mark an expected action as successfully completed for this
    sample.
    """
    _h._complete_action_sample(name, true, _sample_id)

  fun fail_action(name: String) =>
    """
    Mark an action as failed for this sample. The sample fails
    immediately.
    """
    _h._complete_action_sample(name, false, _sample_id)

  fun dispose_when_done(disposable: DisposableActor) =>
    """
    Pass an actor to be disposed when this sample finishes.
    """
    _h._dispose_when_done_sample(disposable, _sample_id)

  fun _format_loc(loc: SourceLoc): String =>
    loc.file() + ":" + loc.line().string() + ": "

  fun _print_array[A: Stringable #read](array: ReadSeq[A]): String =>
    "[len=" + array.size().string() + ": " + ", ".join(array.values()) + "]"
