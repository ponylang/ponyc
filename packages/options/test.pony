use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestLongOptions)
    test(_TestShortOptions)
    test(_TestCombineShortOptions)
    test(_TestCombineShortArg)
    test(_TestArgLeadingDash)

primitive TestOptions
  fun from(renv: Env val, args: Array[String] val): Options =>
    let env = recover val Env.create(renv.root, renv.input, renv.out, renv.err,
      args, None) end
    Options(env)

class iso _TestLongOptions is UnitTest
  """
  Long options start with two leading dashes, and can be lone, have a following
  arg, or combined arg with =.
  """
  fun name(): String => "options/Options.longOptions"

  fun apply(h: TestHelper) =>
    let options = TestOptions.from(h.env, recover ["--none", "--i64", "12345",
      "--f64=67.890"] end)
    var none: Bool = false
    var i64: I64 = -1
    var f64: F64 = -1
    options
      .add("none", "n", None, Optional)
      .add("i64", "i", I64Argument, Optional)
      .add("f64", "f", F64Argument, Optional)
    for option in options do
      match option
      | ("none", let arg: None) => none = true
      | ("i64", let arg: I64) => i64 = arg
      | ("f64", let arg: F64) => f64 = arg
      end
    end

    h.assert_eq[Bool](true, none)
    h.assert_eq[I64](12345, i64)
    h.assert_eq[F64](67.890, f64)

class iso _TestShortOptions is UnitTest
  """
  Short options start with a single leading dash, and can be lone, have a
  following arg, or combined arg with =.
  """
  fun name(): String => "options/Options.shortOptions"

  fun apply(h: TestHelper) =>
    let options = TestOptions.from(h.env, recover val ["-n", "-i", "12345",
      "-f67.890"] end)
    var none: Bool = false
    var i64: I64 = -1
    var f64: F64 = -1
    options
      .add("none", "n", None, Optional)
      .add("i64", "i", I64Argument, Optional)
      .add("f64", "f", F64Argument, Optional)
    for option in options do
      match option
      | ("none", let arg: None) => none = true
      | ("i64", let arg: I64) => i64 = arg
      | ("f64", let arg: F64) => f64 = arg
      else
        h.fail("Invalid option reported")
      end
    end

    h.assert_eq[Bool](true, none)
    h.assert_eq[I64](12345, i64)
    h.assert_eq[F64](67.890, f64)

class iso _TestCombineShortOptions is UnitTest
  """
  Short options can be combined into one string with a single leading dash.
  """
  fun name(): String => "options/Options.combineShort"

  fun apply(h: TestHelper) =>
    let options = TestOptions.from(h.env, recover val ["-ab"] end)
    var aaa: Bool = false
    var bbb: Bool = false
    options
      .add("aaa", "a", None, Optional)
      .add("bbb", "b", None, Optional)
    for option in options do
      match option
      | ("aaa", _) => aaa = true
      | ("bbb", _) => bbb = true
      end
    end

    h.assert_eq[Bool](true, aaa)
    h.assert_eq[Bool](true, bbb)

class iso _TestCombineShortArg is UnitTest
  """
  Short options can be combined up to the first option that takes an argument.
  """
  fun name(): String => "options/Options.combineShortArg"

  fun apply(h: TestHelper) =>
    let options =
      TestOptions.from(h.env, recover val ["-ab99", "-ac", "99"] end)
    var aaa: Bool = false
    var bbb: I64 = -1
    var ccc: I64 = -1
    options
      .add("aaa", "a", None, Optional)
      .add("bbb", "b", I64Argument, Optional)
      .add("ccc", "c", I64Argument, Optional)
    for option in options do
      match option
      | ("aaa", _) => aaa = true
      | ("bbb", let arg: I64) => bbb = arg
      | ("ccc", let arg: I64) => ccc = arg
      end
    end

    h.assert_eq[Bool](true, aaa)
    h.assert_eq[I64](99, bbb)
    h.assert_eq[I64](99, ccc)

class iso _TestArgLeadingDash is UnitTest
  """
  Arguments can only start with a leading dash if they are separated from
  the option using '=', otherwise they will be interpreted as named options.
  """
  fun name(): String => "options/Options.testArgLeadingDash"

  fun apply(h: TestHelper) =>
    let options =
      TestOptions.from(h.env, recover val ["--aaa", "-2", "--bbb=-2"] end)
    var aaa: I64 = -1
    var bbb: I64 = -1
    options
      .add("aaa", "c", I64Argument, Optional)
      .add("bbb", "b", I64Argument, Optional)
      .add("ttt", "2", None, Optional)  // to ignore the -2
    for option in options do
      match option
      | ("aaa", let arg: I64) => aaa = arg
      | ("bbb", let arg: I64) => bbb = arg
      end
    end

    h.assert_eq[I64](-1, aaa)
    h.assert_eq[I64](-2, bbb)
