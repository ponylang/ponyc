use "constrained_types"
use "pony_test"

class \nodoc\ iso _TestMaxSpawnRejectsZero is UnitTest
  fun name(): String => "net/MaxSpawnRejectsZero"

  fun apply(h: TestHelper) =>
    match \exhaustive\ MakeMaxSpawn(0)
    | let _: MaxSpawn =>
      h.fail("MakeMaxSpawn(0) should return ValidationFailure")
    | let _: ValidationFailure =>
      h.assert_true(true)
    end

class \nodoc\ iso _TestMaxSpawnAcceptsBoundary is UnitTest
  fun name(): String => "net/MaxSpawnAcceptsBoundary"

  fun apply(h: TestHelper) =>
    match \exhaustive\ MakeMaxSpawn(1)
    | let m: MaxSpawn =>
      h.assert_eq[U32](1, m())
    | let _: ValidationFailure =>
      h.fail("MakeMaxSpawn(1) should succeed")
    end

    match \exhaustive\ MakeMaxSpawn(U32.max_value())
    | let m: MaxSpawn =>
      h.assert_eq[U32](U32.max_value(), m())
    | let _: ValidationFailure =>
      h.fail("MakeMaxSpawn(U32.max_value()) should succeed")
    end

class \nodoc\ iso _TestDefaultMaxSpawn is UnitTest
  fun name(): String => "net/DefaultMaxSpawn"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](100_000, DefaultMaxSpawn()())

class \nodoc\ iso _TestReadBufferSizeRejectsZero is UnitTest
  fun name(): String => "net/ReadBufferSizeRejectsZero"

  fun apply(h: TestHelper) =>
    match \exhaustive\ MakeReadBufferSize(0)
    | let _: ReadBufferSize =>
      h.fail("MakeReadBufferSize(0) should return ValidationFailure")
    | let _: ValidationFailure =>
      h.assert_true(true)
    end

class \nodoc\ iso _TestReadBufferSizeAcceptsBoundary is UnitTest
  fun name(): String => "net/ReadBufferSizeAcceptsBoundary"

  fun apply(h: TestHelper) =>
    match \exhaustive\ MakeReadBufferSize(1)
    | let r: ReadBufferSize =>
      h.assert_eq[USize](1, r())
    | let _: ValidationFailure =>
      h.fail("MakeReadBufferSize(1) should succeed")
    end

    match \exhaustive\ MakeReadBufferSize(USize.max_value())
    | let r: ReadBufferSize =>
      h.assert_eq[USize](USize.max_value(), r())
    | let _: ValidationFailure =>
      h.fail("MakeReadBufferSize(USize.max_value()) should succeed")
    end

class \nodoc\ iso _TestDefaultReadBufferSize is UnitTest
  fun name(): String => "net/DefaultReadBufferSize"

  fun apply(h: TestHelper) =>
    h.assert_eq[USize](16384, DefaultReadBufferSize()())

class \nodoc\ iso _TestConnectionTimeoutValidationRejectsZero is UnitTest
  fun name(): String => "net/ConnectionTimeoutValidationRejectsZero"

  fun apply(h: TestHelper) =>
    match \exhaustive\ MakeConnectionTimeout(0)
    | let _: ConnectionTimeout =>
      h.fail("MakeConnectionTimeout(0) should return ValidationFailure")
    | let _: ValidationFailure =>
      h.assert_true(true)
    end

class \nodoc\ iso _TestConnectionTimeoutValidationAcceptsBoundary is UnitTest
  fun name(): String => "net/ConnectionTimeoutValidationAcceptsBoundary"

  fun apply(h: TestHelper) =>
    match \exhaustive\ MakeConnectionTimeout(1)
    | let ct: ConnectionTimeout =>
      h.assert_eq[U64](1, ct())
    | let _: ValidationFailure =>
      h.fail("MakeConnectionTimeout(1) should succeed")
    end

    match \exhaustive\ MakeConnectionTimeout(U64.max_value() / 1_000_000)
    | let ct: ConnectionTimeout =>
      h.assert_eq[U64](U64.max_value() / 1_000_000, ct())
    | let _: ValidationFailure =>
      h.fail(
        "MakeConnectionTimeout(U64.max_value() / 1_000_000) should succeed")
    end

    match \exhaustive\ MakeConnectionTimeout((U64.max_value() / 1_000_000) + 1)
    | let _: ConnectionTimeout =>
      h.fail(
        "MakeConnectionTimeout(max + 1) should return ValidationFailure")
    | let _: ValidationFailure =>
      h.assert_true(true)
    end

class \nodoc\ iso _TestTimerDurationValidationRejectsZero is UnitTest
  fun name(): String => "net/TimerDurationValidationRejectsZero"

  fun apply(h: TestHelper) =>
    match \exhaustive\ MakeTimerDuration(0)
    | let _: TimerDuration =>
      h.fail("MakeTimerDuration(0) should return ValidationFailure")
    | let _: ValidationFailure =>
      h.assert_true(true)
    end

class \nodoc\ iso _TestTimerDurationValidationAcceptsBoundary is UnitTest
  fun name(): String => "net/TimerDurationValidationAcceptsBoundary"

  fun apply(h: TestHelper) =>
    match \exhaustive\ MakeTimerDuration(1)
    | let td: TimerDuration =>
      h.assert_eq[U64](1, td())
    | let _: ValidationFailure =>
      h.fail("MakeTimerDuration(1) should succeed")
    end

    match \exhaustive\ MakeTimerDuration(U64.max_value() / 1_000_000)
    | let td: TimerDuration =>
      h.assert_eq[U64](U64.max_value() / 1_000_000, td())
    | let _: ValidationFailure =>
      h.fail("MakeTimerDuration(U64.max_value() / 1_000_000) should succeed")
    end

    match \exhaustive\ MakeTimerDuration((U64.max_value() / 1_000_000) + 1)
    | let _: TimerDuration =>
      h.fail(
        "MakeTimerDuration(max + 1) should return ValidationFailure")
    | let _: ValidationFailure =>
      h.assert_true(true)
    end
