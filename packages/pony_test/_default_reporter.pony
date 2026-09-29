primitive _StandardReporter
  """
  Sentinel value for the `reporter` parameter of `PonyTest.create`.
  Means "use the standard console reporter."
  """

actor _DefaultReporter
  """
  Prints the standard PonyTest console output. Used when no custom
  reporter is supplied.
  """
  let _out: OutStream
  let _verbose: Bool
  let _no_prog: Bool
  var _started: USize = 0
  var _finished: USize = 0

  new create(out: OutStream, verbose: Bool, no_prog: Bool) =>
    _out = out
    _verbose = verbose
    _no_prog = no_prog

  be test_started(name: String) =>
    _started = _started + 1
    if not _no_prog then
      _out.print(
        _started.string() + " test" + _plural(_started) +
          " started, " + _finished.string() + " complete: " +
          name + " started")
    end

  be test_complete(result: TestResult val) =>
    _finished = _finished + 1
    if not _no_prog then
      _out.print(
        _started.string() + " test" + _plural(_started) +
          " started, " + _finished.string() + " complete: " +
          result.name + " complete")
    end

  be testing_complete(results: Array[TestResult val] val) =>
    var pass_count: USize = 0
    var fail_count: USize = 0

    for r in results.values() do
      if r.passed then
        _out.print(
          _Color.green() + "---- Passed: " + r.name + _Color.reset())
        pass_count = pass_count + 1
      else
        _out.print(
          _Color.red() + "**** FAILED: " + r.name + _Color.reset())
        fail_count = fail_count + 1
      end

      if _verbose or (not r.passed) then
        for msg in r.log.values() do
          _out.print(msg)
        end
      end
    end

    _out.print("----")
    _out.print("---- " + results.size().string() + " test" +
      _plural(results.size()) + " ran.")
    _out.print(_Color.green() + "---- Passed: " + pass_count.string() +
      _Color.reset())

    if fail_count > 0 then
      _out.print(_Color.red() + "**** FAILED: " + fail_count.string() +
        " test" + _plural(fail_count) + ", listed below:" +
        _Color.reset())
      for r in results.values() do
        if not r.passed then
          _out.print(
            _Color.red() + "**** FAILED: " + r.name + _Color.reset())
        end
      end
    end

  fun _plural(n: USize): String =>
    if n == 1 then "" else "s" end
