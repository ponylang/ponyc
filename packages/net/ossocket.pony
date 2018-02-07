
primitive _OSSocket
  """
  TODO
  """

  fun get_so_error(fd: U32): (U32, U32) =>
    getsockopt(fd, OSSockOpt.sol_socket(), OSSockOpt.so_error())

  fun get_so_rcvbuf(fd: U32): (U32, U32) =>
    getsockopt(fd, OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())

  fun get_so_sndbuf(fd: U32): (U32, U32) =>
    getsockopt(fd, OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf())

  fun set_so_rcvbuf(fd: U32, bufsize: I32): U32 =>
    var word: Array[U8] ref = [
      (bufsize and 0xFF).u8(); ((bufsize >> 8) and 0xFF).u8()
      ((bufsize >> 16) and 0xFF).u8(); ((bufsize >> 24) and 0xFF).u8()] //LE
    set_so(fd, OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), word)

  fun set_so_sndbuf(fd: U32, bufsize: I32): U32 =>
    var word: Array[U8] ref = [
      (bufsize and 0xFF).u8(); ((bufsize >> 8) and 0xFF).u8()
      ((bufsize >> 16) and 0xFF).u8(); ((bufsize >> 24) and 0xFF).u8()] //LE
    set_so(fd, OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf(), word)


/**************************************/

  fun getsockopt(fd: U32, level: I32, option_name: I32, option: (None | Array[U8]) = None): (U32, U32) =>
    match option
    | None =>
      var word: Array[U8] ref = [0;0;0;0]
      (let errno: U32, _) = get_so(fd, level, option_name, word)
      if errno == 0 then
        try
          (errno, _to_u32(word(3)?, word(2)?, word(1)?, word(0)?)) //LE
        else
          (1, 0)
        end
      else
        (errno, 0)
      end
    | let oa: Array[U8] =>
      get_so(fd, level, option_name, oa)
    end

  fun setsockopt(fd: U32, level: I32, option_name: I32, option: (I32 | Array[U8])): U32 =>
    match option
    | let opt: I32 =>
      var word: Array[U8] ref = [
        (opt and 0xFF).u8(); ((opt >> 8) and 0xFF).u8()
        ((opt >> 16) and 0xFF).u8(); ((opt >> 24) and 0xFF).u8()
        ] //LE
      set_so(fd, level, option_name, word)
    | let oa: Array[U8] =>
      set_so(fd, level, option_name, oa)
    end

  fun get_so(fd: U32, level: I32, option_name: I32, option: Array[U8]): (U32, U32) =>
    var option_size: U32 = option.size().u32()
    let result: U32 = @pony_os_getsockopt[U32](fd, level, option_name,
       option.cpointer(), addressof option_size)

    if result == 0 then
      (result, option_size)
    else
      (result, U32(0))
    end

  fun set_so(fd: U32, level: I32, option_name: I32, option: Array[U8]): U32 =>
    var option_size: U32 = option.size().u32()

    @pony_os_setsockopt[U32](fd, level, option_name,
       option.cpointer(), addressof option_size)

  fun _to_u32(a: U8, b: U8, c: U8, d: U8): U32 =>
    (a.u32() << 24) or (b.u32() << 16) or (c.u32() << 8) or d.u32()

/**************************************/
