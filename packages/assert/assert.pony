"""
# Assert package

Contains runtime assertions. If you are looking for assertion that only run
when your code was compiled with the `debug` flag, check out `Assert`. For
assertions that are always enabled, check out `Fact`.
"""

use @pony_os_stderr[Pointer[U8]]()
use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)

primitive Assert
  """
  This is a debug only assertion. If the test is false, it will print any
  supplied error message to stderr and raise an error.
  """
  fun apply(test: Bool, msg: String = "") ? =>
    ifdef debug then
      Fact(test, msg)?
    end

primitive Fact
  """
  This is an assertion that is always enabled. If the test is false, it will
  print any supplied error message to stderr and raise an error.
  """
  fun apply(test: Bool, msg: String = "") ? =>
    if not test then
      if msg.size() > 0 then
        @fprintf(@pony_os_stderr(), "%s\n".cstring(), msg.cstring())
      end
      error
    end
