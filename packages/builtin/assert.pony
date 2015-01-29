primitive Assert
  """
  This is a debug only assertion. If the test is false, it will print any
  supplied error message to stderr and raise an error.
  """
  fun apply(test: Bool, msg: String = "") ? =>
    if Platform.debug() and not test then
      if msg.size() > 0 then
        let output = msg + "\n"
        let err = @os_stderr[Pointer[U8]]()
        @fwrite[U64](output.cstring(), U64(1), output.size(), err)
      end
      error
    end

primitive Check
  """
  This is an assertion that is always enabled. If the test is false, it will
  print any supplied error message to stderr and raise an error.
  """
  fun apply(test: Bool, msg: String = "") ? =>
    if not test then
      if msg.size() > 0 then
        let output = msg + "\n"
        let err = @os_stderr[Pointer[U8]]()
        @fwrite[U64](output.cstring(), U64(1), output.size(), err)
      end
      error
    end
