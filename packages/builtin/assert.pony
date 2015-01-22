primitive Assert
  fun apply(test: Bool, msg: String = "ASSERT") ? =>
    if Platform.debug() and not test then
      let err = @os_stderr[Pointer[U8]]()
      @fwrite[U64](msg.cstring(), U64(1), msg.size(), err)
      @fwrite[U64]("\n".cstring(), U64(1), U64(1), err)
      error
    end
