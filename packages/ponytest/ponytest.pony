use "collections"
use "options"
use "regex"

trait UnitTest
  fun name(): String
  fun exclusion_group(): String => ""
  fun long_test(): Bool => false
  fun ref apply(t: TestHelper): Bool ?


type _TestRecord is (String, Bool, (Array[String] val | None))


actor PonyTest
  let _testers: Map[String, _Tester]
  let _records: Array[_TestRecord]
  let _env: Env
  var _do_nothing: Bool = false
  var _filter: (Regex | None) = None
  var _log_all: Bool = false
  var _sequential: Bool = false
  var _started: U64 = 0
  var _finished: U64 = 0
  var _all_started: Bool = false

  new create(env: Env) =>
    _testers = Map[String, _Tester]
    _records = Array[_TestRecord]
    _env = env
    _process_opts()

  be apply(test: UnitTest iso) =>
    if _do_nothing then
      return
    end

    var name = test.name()

    // Ignore any tests that don't satisfy our filter, if we have one
    match _filter
    | var filter: Regex =>
      if not filter(name) then
        return
      end
    end

    var index = _records.size()
    _records.push((name, false, None))
    
    _started = _started + 1
    _env.out.print("Tests: " + _started.string() + " started, " +
      _finished.string() + " complete")

    var tester = _tester_for_group(test.exclusion_group())
    tester(consume test, index)

  fun ref _tester_for_group(group: String): _Tester =>
    var g = group

    if _sequential then
      // Use the same tester for all tests
      g = ""
    elseif group == "" then
      // No exclusion group, this test gets its own tester
      return _Tester(this)
    end

    try
      _testers(g)
    else
      // No tester made for group yet, make one
      var t = _Tester(this)
      _testers(g) = t
      t
    end

  be _test_result(index: U64, pass: Bool, log: Array[String] val) =>
    try
      _records(index) = (_records(index)._1, pass, consume log)
    end

    _finished = _finished + 1
    _env.out.print("Tests: " + _started.string() + " started, " +
      _finished.string() + " complete")
    
    if _all_started and (_finished == _started) then
      _print_report()
    end

  be complete() =>
    if _do_nothing then
      return
    end

    if _started == 0 then
      _env.out.print("No tests found")
      return
    end

    _all_started = true
    if _finished == _started then
      _print_report()
    end

  fun ref _process_opts() =>
    var opts = Options(_env)

    opts.usage_text(
      """
      ponytest [OPTIONS]

      Options:
      """)
      .add("sequential", "s", "Tests run sequentially.", None)
      .add("log", "l", "Show logs for all tests run.", None)
      .add("filter", "f", "Only run the tests matching the given regex.",
        StringArgument)

    for option in opts do
      match option
      | ("sequential", None) => _sequential = true
      | ("log", None) => _log_all = true
      | ("filter", var arg: String) =>
        try
          _filter = Regex(arg)
        else
          _env.out.print("Invalid regular expression \"" + arg + "\"")
          _do_nothing = true
        end
      | (None, None) => None
      else
        opts.usage()
        _do_nothing = true
      end
    end

  fun _print_report() =>
    var pass_count: U64 = 0
    var fail_count: U64 = 0

    try
      for rec in _records.values() do
        var show_log = _log_all

        if rec._2 then
          // TODO: print colour
          _env.out.print("---- Test passed: " + rec._1)  // green
          pass_count = pass_count + 1
        else
          //_env.out.print("\x1b[31m**** Test FAILED: " + rec._1 + "\x1b[39m")
          _env.out.print("**** Test FAILED: " + rec._1)  // red
          fail_count = fail_count + 1
          show_log = true
        end

       if show_log then
         match rec._3
         | let log: Array[String] val =>
            for msg in log.values() do
              _env.out.print(msg)
            end
          end
        end
      end
    end

    _env.out.print("----")
    _env.out.print(_records.size().string() + " tests run. " +
      pass_count.string() + " passed, " + fail_count.string() + " failed.")

    _env.exitcode(if fail_count == 0 then 0 else -1 end)
