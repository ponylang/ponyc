use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestNanos)
    test(_TestPosixDate)
    test(_TestPosixDateClamping)
    test.property[(I64, I64)](_TestPosixDateFieldsInRange)
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

class \nodoc\ iso _TestPosixDateClamping is UnitTest
  fun name(): String => "time/PosixDate/clamping"

  fun apply(h: TestHelper) =>
    // The clamping boundary is platform-dependent: year 3000 on Windows
    // (gmtime_s limit), year 2147483647 elsewhere.
    ifdef windows then
      let max_date = PosixDate(32535215999)
      h.assert_eq[I32](3000, max_date.year)
      h.assert_eq[I32](12, max_date.month)
      h.assert_eq[I32](31, max_date.day_of_month)
      h.assert_eq[I32](23, max_date.hour)
      h.assert_eq[I32](59, max_date.min)
      h.assert_eq[I64](32535215999, max_date.time())

      let clamped = PosixDate(32535216000)
      h.assert_eq[I32](3000, clamped.year)
      h.assert_eq[I32](12, clamped.month)
      h.assert_eq[I32](31, clamped.day_of_month)
      h.assert_eq[I64](32535215999, clamped.time())

      // Nanoseconds pushing past the boundary
      let nsec_over = PosixDate(32535215999, 1_000_000_000)
      h.assert_eq[I32](3000, nsec_over.year)
      h.assert_eq[I64](32535215999, nsec_over.time())
    else
      let max_date = PosixDate(67767976233532799)
      h.assert_eq[I32](2147483647, max_date.year)
      h.assert_eq[I32](12, max_date.month)
      h.assert_eq[I32](31, max_date.day_of_month)
      h.assert_eq[I32](23, max_date.hour)
      h.assert_eq[I32](59, max_date.min)
      h.assert_eq[I64](67767976233532799, max_date.time())

      let clamped = PosixDate(67767976233532800)
      h.assert_eq[I32](2147483647, clamped.year)
      h.assert_eq[I32](12, clamped.month)
      h.assert_eq[I32](31, clamped.day_of_month)
      h.assert_eq[I64](67767976233532799, clamped.time())

      // Nanoseconds pushing past the boundary
      let nsec_over = PosixDate(67767976233532799, 1_000_000_000)
      h.assert_eq[I32](2147483647, nsec_over.year)
      h.assert_eq[I64](67767976233532799, nsec_over.time())
    end

    // I64.max_value() seconds: clamped on all platforms
    let huge = PosixDate(I64.max_value())
    h.assert_true(huge.year >= 1970)

    // I64.max seconds + I64.max nanoseconds: clamped, no signed overflow
    let both_max = PosixDate(I64.max_value(), I64.max_value())
    h.assert_true(both_max.year >= 1970)

class \nodoc\ iso _TestPosixDateFieldsInRange is Property2[I64, I64]
  """
  For any pair of I64 arguments, PosixDate fields stay within their
  documented ranges.
  """
  fun name(): String => "time/PosixDate/fields-in-range"

  fun gen1(): Generator[I64] =>
    Generators.frequency[I64]([
      as WeightedGenerator[I64]:
      (1, Generators.i64())
      (3, Generators.i64(0, 67767976233532799))
      (1, Generators.one_of[I64]([
        I64(0); I64(1); I64(32535215999); I64(32535216000)
        I64(67767976233532798); I64(67767976233532799)
        I64(67767976233532800); I64.max_value()]))
    ])

  fun gen2(): Generator[I64] =>
    Generators.frequency[I64]([
      as WeightedGenerator[I64]:
      (1, Generators.i64())
      (3, Generators.i64(0, 2_000_000_000))
      (1, Generators.one_of[I64]([
        I64(0); I64(1); I64(999_999_999); I64(1_000_000_000)
        I64.max_value()]))
    ])

  fun ref property2(seconds: I64, nanoseconds: I64, h: PropertyHelper) =>
    let d = PosixDate(seconds, nanoseconds)
    h.assert_true(d.year >= 1970,
      "year " + d.year.string() + " < 1970 for seconds="
        + seconds.string() + " nanoseconds=" + nanoseconds.string())
    h.assert_true((d.month >= 1) and (d.month <= 12),
      "month " + d.month.string() + " out of [1, 12]")
    h.assert_true((d.day_of_month >= 1) and (d.day_of_month <= 31),
      "day_of_month " + d.day_of_month.string() + " out of [1, 31]")
    h.assert_true((d.hour >= 0) and (d.hour <= 23),
      "hour " + d.hour.string() + " out of [0, 23]")
    h.assert_true((d.min >= 0) and (d.min <= 59),
      "min " + d.min.string() + " out of [0, 59]")
    h.assert_true((d.sec >= 0) and (d.sec <= 60),
      "sec " + d.sec.string() + " out of [0, 60]")
    h.assert_true((d.nsec >= 0) and (d.nsec < 1_000_000_000),
      "nsec " + d.nsec.string() + " out of [0, 1000000000)")
    h.assert_true((d.day_of_week >= 0) and (d.day_of_week <= 6),
      "day_of_week " + d.day_of_week.string() + " out of [0, 6]")
    h.assert_true((d.day_of_year >= 0) and (d.day_of_year <= 365),
      "day_of_year " + d.day_of_year.string() + " out of [0, 365]")

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
