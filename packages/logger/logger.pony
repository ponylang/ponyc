"""
# Logger package

Provides basic logging facilities. On construction, takes 3 parameters:

* LogLevel below which, no output will be logged
* OutStream to log to
* Lambda that converts an object of type A to a String for logging

The API for using Logger is an attempt to abide by the Pony philosophy of first,
be correct and secondly, aim for performance. One of the ways that logging can
slow your system down is by having to evaluate expressions to be logged
whether they will be logged or not (based on the level of logging). For example:

logger.log(Warn, name + ": " + reason)

will construct a new String regardless of whether we will end up logging the
message or not.

The Logger API uses boolean short circuiting to avoid this.

logger(Warn) and logger.log(name + ": " + reason)

will not evaluate the expression to be logged unless the log level Warn is at
or above the overall log level for our logging. This is as close as we can get
to zero cost for items that aren't going to end up being logged.

## Example program

The following program will output 'my warn message' and 'my error message' to
STDOUT:

```pony
use "logger"

actor Main
  new create(env: Env) =>
    let logger = Logger[String](
      Warn,
      env.out,
      lambda(a: String): String => a end)

    logger(Debug) and logger.log("my debug message")
    logger(Info) and logger.log("my info message")
    logger(Warn) and logger.log("my warn message")
    logger(Error) and logger.log("my error message")
```
"""

type LogLevel is
  ( Debug
  | Info
  | Warn
  | Error
  )

primitive Debug
  fun apply(): U32 => 0

primitive Info
  fun apply(): U32 => 1

primitive Warn
  fun apply(): U32 => 2

primitive Error
  fun apply(): U32 => 3

class val Logger[A]
  let _level: LogLevel
  let _out: OutStream
  let _f: {(A): String} val

  new val create(level: LogLevel, out: OutStream, f: {(A): String} val) =>
    _level = level
    _out = out
    _f = f

  fun apply(level: LogLevel): Bool =>
    level() >= _level()

  fun log(value: A, loc: SourceLoc = __loc): Bool =>
    let msg = _f(consume value)
    let line_string: String val = loc.line().string()
    let pos_string: String val = loc.pos().string()

    let output = recover String(loc.file().size()
    + line_string.size()
    + pos_string.size()
    + msg.size()
    + 4) end

    output.append(loc.file())
    output.append(":")
    output.append(line_string)
    output.append(":")
    output.append(pos_string)
    output.append(": ")
    output.append(msg)

    _out.print(consume output)
    true
