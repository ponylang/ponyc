primitive _Colors
  fun green(): String => "\x1b[0;32m"
  fun red(): String => "\x1b[0;31m"
  fun yellow(): String => "\x1b[0;33m"
  fun none(): String => "\x1b[0m"

  fun info(msg: String, double: Bool = false): String =>
    green()
      + (if double then "[==========] " else "[----------] " end)
      + none() + msg

  fun warn(msg: String): String =>
    yellow() + "[----------] " + none() + msg

  fun err(msg: String): String =>
    red() + "[----------] " + none() + msg

  fun run(msg: String): String =>
    green() + "[ RUN      ] " + none() + msg

  fun ok(msg: String): String =>
    green() + "[       OK ] " + none() + msg

  fun pass(msg: String): String =>
    green() + "[  PASSED  ] " + none() + msg

  fun fail(msg: String): String =>
    red() + "[  FAILED  ] " + none() + msg
