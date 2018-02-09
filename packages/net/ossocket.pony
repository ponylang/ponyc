
primitive _OSSocket
  """
  Socket type-independent wrapper functions for `getsockopt(2)` and
  `setsockopt(2)` system calls for internal `net` package use.
  """

  fun get_so_error(fd: U32): (U32, U32) =>
    """
    Wrapper for the FFI call `getsockopt(fd, SOL_SOCKET, SO_ERROR, ...)`
    """
    getsockopt_u32(fd, OSSockOpt.sol_socket(), OSSockOpt.so_error())

  fun get_so_rcvbuf(fd: U32): (U32, U32) =>
    """
    Wrapper for the FFI call `getsockopt(fd, SOL_SOCKET, SO_RCVBUF, ...)`
    """
    getsockopt_u32(fd, OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())

  fun get_so_sndbuf(fd: U32): (U32, U32) =>
    """
    Wrapper for the FFI call `getsockopt(fd, SOL_SOCKET, SO_SNDBUF, ...)`
    """
    getsockopt_u32(fd, OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf())

  fun set_so_rcvbuf(fd: U32, bufsize: U32): U32 =>
    """
    Wrapper for the FFI call `setsockopt(fd, SOL_SOCKET, SO_RCVBUF, ...)`
    """
    setsockopt_u32(fd, OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), bufsize)

  fun set_so_sndbuf(fd: U32, bufsize: U32): U32 =>
    """
    Wrapper for the FFI call `setsockopt(fd, SOL_SOCKET, SO_SNDBUF, ...)`
    """
    setsockopt_u32(fd, OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf(), bufsize)


  fun getsockopt(fd: U32, level: I32, option_name: I32, option: Array[U8]): (U32, U32) =>
    """
    General wrapper for sockets to the `getsockopt(2)` system call.

    In case of system call success, this function returns the 2-tuple:
    1. The integer `0`.
    2. The value of the `*(uint32_t)option_length` argument set by
       `getsockopt(2)`.  The caller must use this length to properly
        interpret the bytes written by the kernel into the `option`
        byte array: this length may be smaller than `option`'s
        original size.

    In case of system call failure, this function returns the 2-tuple:
    1. The value of `errno`.
    2. An undefined value that must be ignored.
    """
    get_so(fd, level, option_name, option)

  fun getsockopt_u32(fd: U32, level: I32, option_name: I32): (U32, U32) =>
    var word: Array[U8] ref = [0;0;0;0]
    """
    Wrapper for sockets to the `getsockopt(2)` system call where
    the kernel's returned option value is a C `uint32_t` type / Pony
    type `U32`.

    In case of system call success, this function returns the 2-tuple:
    1. The integer `0`.
    2. The `*option_value` returned by the kernel converted to a Pony `U32`.

    In case of system call failure, this function returns the 2-tuple:
    1. The value of `errno`.
    2. An undefined value that must be ignored.
    """
    (let errno: U32, _) = get_so(fd, level, option_name, word)

    if errno == 0 then
      try
          (errno, bytes4_to_u32(word)?)
      else
        (1, 0)
      end
    else
      (errno, 0)
    end

  fun setsockopt(fd: U32, level: I32, option_name: I32, option: Array[U8]): U32 =>
    """
    General wrapper for sockets to the `setsockopt(2)` system call.

    The caller is responsible for the correct size and byte contents of
    the `option` array for the requested `level` and `option_name`,
    including using the appropriate CPU endian byte order.

    This function returns `0` on success, else the value of `errno` on
    failure.
    """
    set_so(fd, level, option_name, option)

  fun setsockopt_u32(fd: U32, level: I32, option_name: I32, option: U32): U32 =>
    """
    Wrapper for sockets to the `setsockopt(2)` system call where
    the kernel expects an option value of a C `uint32_t` type / Pony
    type `U32`.

    This function returns `0` on success, else the value of `errno` on
    failure.
    """
    var word: Array[U8] ref = u32_to_bytes4(option)
    set_so(fd, level, option_name, word)

  fun get_so(fd: U32, level: I32, option_name: I32, option: Array[U8]): (U32, U32) =>
    """
    Low-level interface to `getsockopt(2)` via `@pony_os_getsockopt[U32]()`.

    In case of system call success, this function returns the 2-tuple:
    1. The integer `0`.
    2. The value of the `*(uint32_t)option_length` argument set by
       `getsockopt(2)`.

    In case of system call failure, `errno` is returned in the first
    element of the 2-tuple, and the second element's value is junk.
    """
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
    """
    Low-level interface to `setsockopt(2)` via `@pony_os_setsockopt[U32]()`.

    This function returns `0` on success, else the value of `errno` on
    failure.
    """
    @pony_os_setsockopt[U32](fd, level, option_name,
       option.cpointer(), option_size)

  fun bytes4_to_u32(b: Array[U8]): U32 ? =>
    ifdef littleendian then
      (b(3)?.u32() << 24) or (b(2)?.u32() << 16) or (b(1)?.u32() << 8) or b(0)?.u32()
    else
      (b(0)?.u32() << 24) or (b(1)?.u32() << 16) or (b(2)?.u32() << 8) or b(3)?.u32()
    end

  fun u32_to_bytes4(option: U32): Array[U8] =>
    ifdef littleendian then
      [ (option and 0xFF).u8(); ((option >> 8) and 0xFF).u8()
        ((option >> 16) and 0xFF).u8(); ((option >> 24) and 0xFF).u8() ]
    else
      [ ((option >> 24) and 0xFF).u8(); ((option >> 16) and 0xFF).u8()
        ((option >> 8) and 0xFF).u8(); (option and 0xFF).u8() ]
    end