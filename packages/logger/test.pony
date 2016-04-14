use "ponytest"
use "promises"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestError)
    test(_TestWarn)
    test(_TestInfo)
    test(_TestFine)
    test(_TestObjectLogging)

class iso _TestError is _StringLoggerTest
  fun name(): String => "logger/error"

  fun level(): LogLevel => Error

  fun tag expected(): String =>
    "error message\n"

  fun log(logger: Logger[String]) =>
    logger(Error) and logger.log("error message")
    logger(Warn) and logger.log("warn message")
    logger(Info) and logger.log("info message")
    logger(Fine) and logger.log("fine message")

class iso _TestWarn is _StringLoggerTest
  fun name(): String => "logger/warn"

  fun level(): LogLevel => Warn

  fun tag expected(): String =>
    "error message\nwarn message\n"

  fun log(logger: Logger[String]) =>
    logger(Error) and logger.log("error message")
    logger(Warn) and logger.log("warn message")
    logger(Info) and logger.log("info message")
    logger(Fine) and logger.log("fine message")

class iso _TestInfo is _StringLoggerTest
  fun name(): String => "logger/info"

  fun level(): LogLevel => Info

  fun tag expected(): String =>
    "error message\nwarn message\ninfo message\n"

  fun log(logger: Logger[String]) =>
    logger(Error) and logger.log("error message")
    logger(Warn) and logger.log("warn message")
    logger(Info) and logger.log("info message")
    logger(Fine) and logger.log("fine message")

class iso _TestFine is _StringLoggerTest
  fun name(): String => "logger/fine"

  fun level(): LogLevel => Fine

  fun tag expected(): String =>
    "error message\nwarn message\ninfo message\nfine message\n"

  fun log(logger: Logger[String]) =>
    logger(Error) and logger.log("error message")
    logger(Warn) and logger.log("warn message")
    logger(Info) and logger.log("info message")
    logger(Fine) and logger.log("fine message")

class _TestObjectLogging is _LoggerTest[U64]
  fun name(): String => "logger/object"

  fun logger(level': LogLevel,
    stream': OutStream,
    formatter': LogFormatter): Logger[U64]
  =>
    Logger[U64](level',
      stream',
      lambda(a: U64): String => a.string() end,
      formatter')

  fun level(): LogLevel => Fine

  fun tag expected(): String => "42\n"

  fun log(logger': Logger[U64]) =>
    logger'(Error) and logger'.log(U64(42))

primitive _TestFormatter is LogFormatter
  fun apply(msg: String, loc: SourceLoc): String =>
    msg + "\n"

actor _TestStream is OutStream
  let _output: String ref = String
  let _h: TestHelper
  let _promise: Promise[String]

  new create(h: TestHelper, promise: Promise[String]) =>
    _h = h
    _promise = promise

  be print(data: ByteSeq) =>
    _collect(data)

  be write(data: ByteSeq) =>
    _collect(data)

  be printv(data: ByteSeqIter) =>
    for bytes in data.values() do
      _collect(bytes)
    end

  be writev(data: ByteSeqIter) =>
    for bytes in data.values() do
      _collect(bytes)
    end

  fun ref _collect(data: ByteSeq) =>
    _output.append(data)

  be logged() =>
    let s: String = _output.clone()
    _promise(s)

trait _LoggerTest[A] is UnitTest
  fun apply(h: TestHelper) =>
    let promise = Promise[String]
    promise.next[None](recover this~_fulfill(h) end)

    let stream = _TestStream(h, promise)

    log(logger(level(), stream, _TestFormatter))

    stream.logged()
    h.long_test(2_000_000_000) // 2 second timeout

  fun tag _fulfill(h: TestHelper, value: String) =>
    h.assert_eq[String](value, expected())
    h.complete(true)

  fun timed_out(h: TestHelper) =>
    h.complete(false)

  fun logger(level': LogLevel,
    stream': OutStream,
    formatter': LogFormatter): Logger[A]

  fun level(): LogLevel

  fun tag expected(): String

  fun log(logger': Logger[A])

trait _StringLoggerTest is _LoggerTest[String]
  fun logger(level': LogLevel,
    stream': OutStream,
    formatter': LogFormatter): Logger[String]
  =>
    StringLogger(level', stream', formatter')
