"""
# Assert package

Contains runtime assertions. If you are looking for assertion that only run
when your code was compiled with the `debug` flag, check out `Assert`. For
assertions that are always enabled, check out `Fact`.
"""

primitive Assert
  """
  This is a debug only assertion. If the test is false, it will print any
  supplied error message to stderr and raise an error.
  """
  fun apply(test: Bool, msg: String = "") ? =>
    ifdef debug then
      Fact(test, msg)
    end

primitive Fact
  """
  This is an assertion that is always enabled. If the test is false, it will
  print any supplied error message to stderr and raise an error.
  """
  fun apply(test: Bool, msg: String = "") ? =>
    if not test then
      if msg.size() > 0 then
        @fprintf[I32](@pony_os_stderr[Pointer[U8]](), "%s\n".cstring(),
          msg.null_terminated().cstring())
      end
      error
    end
