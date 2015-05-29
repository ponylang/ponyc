use "term"

class _TestRecord
  """
  Store and report the result and log from a single test.
  """

  let _env: Env
  let _name: String
  var _pass: Bool = false
  var _log: (Array[String] val | None) = None

  new create(env: Env, name: String) =>
    _env = env
    _name = name

  fun ref _result(pass: Bool, log: Array[String] val) =>
    """
    Our test has completed, store the result.
    """
    _pass = pass
    _log = log

  fun _report(log_all: Bool): Bool =>
    """
    Print our test summary, including the log if appropriate.
    The log_all parameter indicates whether we've been told to print logs for
    all tests. The default is to only print logs for tests that fail.
    Returns our pass / fail status.
    """
    var show_log = log_all

    if _pass then
      _env.out.print(ANSI.bright_green() + "---- Passed: " + _name +
        ANSI.reset())
    else
      _env.out.print(ANSI.bright_red() + "**** FAILED: " + _name +
        ANSI.reset())
      show_log = true
    end

    if show_log then
      match _log
      | let log: Array[String] val =>
        // Print the log. Simply print each string in the array.
        for msg in log.values() do
          _env.out.print(msg)
        end
      end
    end

    _pass

  fun _list_failed() =>
    """
    Print our test name out in the list of failed test, if we failed.
    """
    if not _pass then
      _env.out.print(ANSI.bright_red() + "**** FAILED: " + _name +
        ANSI.reset())
    end
