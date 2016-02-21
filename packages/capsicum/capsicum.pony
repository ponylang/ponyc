primitive Cap
  """
  The Capsicum rights.
  """
  fun enter(): Bool =>
    """
    This places the current process into capability mode, a mode of execution
    in which processes may only issue system calls operating on file
    descriptors or reading limited global system state. Access to global name
    spaces, such as file system or IPC name spaces, is prevented.
    """
    ifdef freebsd or "capsicum" then
      @cap_enter[I32]() == 0
    else
      false
    end

  fun read(): U64 => _id(0, 1 << 0)
  fun write(): U64 => _id(0, 1 << 1)
  fun seek_tell(): U64 => _id(0, 1 << 2)
  fun seek(): U64 => _id(0, 1 << 3) or seek_tell()
  fun pread(): U64 => seek() or read()
  fun pwrite(): U64 => seek() or write()

  fun mmap(): U64 => _id(0, 1 << 4)
  fun mmap_r(): U64 => mmap() or seek() or read()
  fun mmap_w(): U64 => mmap() or seek() or write()
  fun mmap_x(): U64 => mmap() or seek() or _id(0, 1 << 5)
  fun mmap_rw(): U64 => mmap_r() or mmap_w()
  fun mmap_rx(): U64 => mmap_r() or mmap_x()
  fun mmap_wx(): U64 => mmap_w() or mmap_x()
  fun mmap_rwx(): U64 => mmap_r() or mmap_w() or mmap_x()

  fun creat(): U64 => _id(0, 1 << 6)
  fun fexecve(): U64 => _id(0, 1 << 7)
  fun fsync(): U64 => _id(0, 1 << 8)
  fun ftruncate(): U64 => _id(0, 1 << 9)
  fun lookup(): U64 => _id(0, 1 << 10)
  fun fchdir(): U64 => _id(0, 1 << 11)
  fun fchflags(): U64 => _id(0, 1 << 12)
  fun chflagsat(): U64 => fchflags() or lookup()
  fun fchmod(): U64 => _id(0, 1 << 13)
  fun fchmodat(): U64 => fchmod() or lookup()
  fun fchown(): U64 => _id(0, 1 << 14)
  fun fchownat(): U64 => fchown() or lookup()
  fun fcntl(): U64 => _id(0, 1 << 15)
  fun flock(): U64 => _id(0, 1 << 16)
  fun fpathconf(): U64 => _id(0, 1 << 17)
  fun fsck(): U64 => _id(0, 1 << 18)
  fun fstat(): U64 => _id(0, 1 << 19)
  fun fstatat(): U64 => fstat() or lookup()
  fun fstatfs(): U64 => _id(0, 1 << 20)
  fun futimes(): U64 => _id(0, 1 << 21)
  fun futimesat(): U64 => futimes() or lookup()

  fun linkat(): U64 => _id(0, 1 << 22) or lookup()
  fun mkdirat(): U64 => _id(0, 1 << 23) or lookup()
  fun mkfifoat(): U64 => _id(0, 1 << 24) or lookup()
  fun mknodat(): U64 => _id(0, 1 << 25) or lookup()
  fun renameat(): U64 => _id(0, 1 << 26) or lookup()
  fun symlinkat(): U64 => _id(0, 1 << 27) or lookup()
  fun unlinkat(): U64 => _id(0, 1 << 28) or lookup()

  fun accept(): U64 => _id(0, 1 << 29)
  fun bind(): U64 => _id(0, 1 << 30)
  fun connect(): U64 => _id(0, 1 << 31)
  fun getpeername(): U64 => _id(0, 1 << 32)
  fun getsockname(): U64 => _id(0, 1 << 33)
  fun getsockopt(): U64 => _id(0, 1 << 34)
  fun listen(): U64 => _id(0, 1 << 35)
  fun peeloff(): U64 => _id(0, 1 << 36)
  fun recv(): U64 => read()
  fun send(): U64 => write()
  fun setsockopt(): U64 => _id(0, 1 << 37)
  fun shutdown(): U64 => _id(0, 1 << 38)
  fun bindat(): U64 => _id(0, 1 << 39) or lookup()
  fun connectat(): U64 => _id(0, 1 << 40) or lookup()

  fun sock_client(): U64 =>
    connect() or getpeername() or getsockname() or getsockopt() or peeloff() or
      recv() or send() or setsockopt() or shutdown()

  fun sock_server(): U64 =>
    accept() or bind() or getpeername() or getsockname() or getsockopt() or
      listen() or peeloff() or recv() or send() or setsockopt() or shutdown()

  fun mac_get(): U64 => _id(1, 1 << 0)
  fun mac_set(): U64 => _id(1, 1 << 1)

  fun sem_getvalue(): U64 => _id(1, 1 << 2)
  fun sem_post(): U64 => _id(1, 1 << 3)
  fun sem_wait(): U64 => _id(1, 1 << 4)

  fun event(): U64 => _id(1, 1 << 5)
  fun kqueue_event(): U64 => _id(1, 1 << 6)
  fun ioctl(): U64 => _id(1, 1 << 7)
  fun ttyhook(): U64 => _id(1, 1 << 8)

  fun pdgetpid(): U64 => _id(1, 1 << 9)
  fun pdwait(): U64 => _id(1, 1 << 10)
  fun pdkill(): U64 => _id(1, 1 << 11)

  fun exattr_delete(): U64 => _id(1, 1 << 12)
  fun exattr_get(): U64 => _id(1, 1 << 13)
  fun exattr_list(): U64 => _id(1, 1 << 14)
  fun exattr_set(): U64 => _id(1, 1 << 15)

  fun acl_check(): U64 => _id(1, 1 << 16)
  fun acl_delete(): U64 => _id(1, 1 << 17)
  fun acl_get(): U64 => _id(1, 1 << 18)
  fun acl_set(): U64 => _id(1, 1 << 19)

  fun kqueue_change(): U64 => _id(1, 1 << 20)
  fun kqueue(): U64 => kqueue_event() or kqueue_change()

  fun _id(idx: U64, bit: U64): U64 =>
    """
    Build a Capsicum ID from an index and a bit position.
    """
    (1 << (57 + idx)) or bit
