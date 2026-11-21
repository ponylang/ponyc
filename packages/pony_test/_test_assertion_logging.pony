class \nodoc\ iso _TestAssertionLogging is UnitTest
  let _verbose: Bool

  new iso create(verbose: Bool) => _verbose = verbose

  fun name(): String =>
    "pony_test/assertion_logging/verbose_" + _verbose.string()

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let args: Array[String] val =
      if _verbose then recover val ["test"; "--verbose"] end
      else recover val ["test"] end
      end
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test(_UnitAssertionLog)
          test(_PropertyAssertionLog)
      end
    PonyTest(_TestEnv(h, args), list, _AssertionLogReporter(h, _verbose))

class \nodoc\ val _LoggedValue is (Stringable & Equatable[_LoggedValue])
  let _h: (TestHelper | PropertyHelper)
  let _value: String

  new val create(h: (TestHelper | PropertyHelper), value: String) =>
    _h = h
    _value = value

  fun eq(that: _LoggedValue): Bool =>
    _h.log("compare " + _value)
    _value == that._value

  fun string(): String =>
    _h.log("format " + _value)
    _value

class \nodoc\ val _LoggedLocation is SourceLoc
  let _h: (TestHelper | PropertyHelper)
  let _name: String

  new val create(h: (TestHelper | PropertyHelper), name: String) =>
    _h = h
    _name = name

  fun file(): String =>
    _h.log("format location " + _name)
    _name

  fun type_name(): String => ""
  fun method_name(): String => ""
  fun line(): USize => 1
  fun pos(): USize => 1

primitive \nodoc\ _CheckAssertionLog
  fun apply(h: (TestHelper | PropertyHelper)) =>
    h.assert_true(true where loc = _LoggedLocation(h, "true"))
    h.assert_false(false where loc = _LoggedLocation(h, "false"))
    h.assert_is[U8](1, 1 where loc = _LoggedLocation(h, "is"))
    h.assert_isnt[U8](1, 2 where loc = _LoggedLocation(h, "isnt"))
    h.assert_ne[U8](1, 2 where loc = _LoggedLocation(h, "ne"))
    h.assert_error({() ? => error } where loc = _LoggedLocation(h, "error"))
    h.assert_no_error({() => None } where loc = _LoggedLocation(h, "no_error"))
    let array: Array[U8] = [1; 2]
    h.assert_array_eq[U8](array, array where loc = _LoggedLocation(h, "array"))
    h.assert_array_eq_unordered[U8](
      array, array where loc = _LoggedLocation(h, "unordered"))
    let pass = _LoggedValue(h, "pass")
    h.assert_eq[_LoggedValue](pass, pass, "first")
    h.assert_eq[_LoggedValue](
      _LoggedValue(h, "fail"),
      _LoggedValue(h, "other"),
      "second")

class \nodoc\ iso _UnitAssertionLog is UnitTest
  fun name(): String => "inner/unit"
  fun apply(h: TestHelper) => _CheckAssertionLog(h)

class \nodoc\ iso _PropertyAssertionLog is UnitTest
  fun name(): String => "inner/property"

  fun apply(h: TestHelper) ? =>
    h.for_all[U8](
      recover Generators.unit[U8](0) end,
      PropertyParams(1 where regression_db' = false))(
        {(u, ph) => _CheckAssertionLog(ph) })?

actor \nodoc\ _AssertionLogReporter is TestReporter
  let _h: TestHelper
  let _verbose: Bool

  new create(h: TestHelper, verbose: Bool) =>
    _h = h
    _verbose = verbose

  be test_started(name: String) => None
  be test_complete(result: TestResult val) => None

  be testing_complete(results: Array[TestResult val] val) =>
    _h.assert_eq[USize](2, results.size())
    for result in results.values() do
      let log = "\n".join(result.log.values())
      let names =
        [ "true"; "false"; "is"; "isnt"; "ne"; "error"; "no_error"
          "array"; "unordered"]
      for name in names.values() do
        let formatted =
          result.log.contains("format location " + name, {(a, b) => a == b })
        _h.assert_eq[Bool](_verbose, formatted, result.name + "/" + name)
      end
      _h.assert_false(result.passed, result.name)
      _h.assert_true(log.contains("compare pass"), result.name)
      _h.assert_true(log.contains("compare fail"), result.name)
      _h.assert_true(log.contains("format fail"), result.name)
      _h.assert_true(log.contains("Assert eq failed. second"), result.name)
      _h.assert_eq[Bool](
        _verbose, log.contains("Assert eq passed. first"), result.name)
      _h.assert_eq[Bool](_verbose, log.contains("format pass"), result.name)
    end
    _h.complete(true)
