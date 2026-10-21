use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestNanos)
    test(_TestPosixDate)
    test(_TestPosixDateFormatLength)

class \nodoc\ iso _TestNanos is UnitTest
  fun name(): String => "time/Nanos"

  fun apply(h: TestHelper) =>
    h.assert_eq[U64](5_000_000_000, Nanos.from_seconds(5))
    h.assert_eq[U64](5_000_000, Nanos.from_millis(5))
    h.assert_eq[U64](5_000, Nanos.from_micros(5))

    h.assert_eq[U64](1_230_000_000, Nanos.from_seconds_f(1.23))
    h.assert_eq[U64](1_230_000, Nanos.from_millis_f(1.23))
    h.assert_eq[U64](1_230, Nanos.from_micros_f(1.23))

class \nodoc\ iso _TestPosixDate is UnitTest
  fun name(): String => "time/PosixDate"

  fun apply(h: TestHelper) =>
    // time is in seconds
    h.assert_eq[I64](0, PosixDate(0, 0).time())
    h.assert_eq[I64](1, PosixDate(1, 0).time())
    // small nanoseconds can not affect time
    h.assert_eq[I64](0, PosixDate(0, 900_000_000).time())
    // big nanoseconds can affect time
    h.assert_eq[I64](1, PosixDate(0, 1_000_000_000).time())
    h.assert_eq[I64](2, PosixDate(1, 1_000_000_000).time())
    // negaive seconds should be changed to zero
    h.assert_eq[I64](0, PosixDate(-1, 0).time())
    h.assert_eq[I64](1, PosixDate(-1, 1_000_000_000).time())
    // negative nanoseconds cannot not affect time
    h.assert_eq[I64](1, PosixDate(1, -1_000_000_000).time())

    // Windows should throw error when %L is in format string
    ifdef windows then
      h.assert_error({() ? =>
        (let seconds, let nanos) = Time.now()
        let d = PosixDate(seconds, nanos)
        d.format("%Y-%m-%d %H:%M:%S.%L")?
      })
    end

class \nodoc\ iso _TestPosixDateFormatLength is UnitTest
  fun name(): String => "time/PosixDate.format/long-output"

  fun apply(h: TestHelper) ? =>
    let date = PosixDate(0)

    let literal: String val = "a" * 10000
    let result = date.format(literal)?
    h.assert_eq[USize](10000, result.size())
    h.assert_eq[String](literal, result)

    let expanded = date.format(recover val "%Y" * 2048 end)?
    h.assert_eq[USize](8192, expanded.size())
    h.assert_eq[String](recover val "1970" * 2048 end, expanded)
