use "files"
use @__cap_rights_init[Pointer[U64]](version: I32, rights: Pointer[U64], ...)
  if freebsd or "capsicum"
use @__cap_rights_clear[Pointer[U64]](rights: Pointer[U64], ...)
  if freebsd or "capsicum"
use @__cap_rights_set[Pointer[U64]](rights: Pointer[U64], ...)
  if freebsd or "capsicum"
use @__cap_rights_get[I32](version: I32, fd: I32, rights: Pointer[U64])
  if freebsd or "capsicum"
use @cap_rights_limit[I32](fd: I32, rights: Pointer[U64])
  if freebsd or "capsicum"
use @cap_rights_merge[None](dst: Pointer[U64], src: Pointer[U64])
  if freebsd or "capsicum"
use @cap_rights_remove[None](dst: Pointer[U64], src: Pointer[U64])
  if freebsd or "capsicum"
use @cap_rights_contains[Bool](big: Pointer[U64] tag, little: Pointer[U64] tag)
  if freebsd or "capsicum"

type CapRights is CapRights0

class CapRights0
  """
  Version 0 of the capsicum cap_rights_t structure.
  """
  var _r0: U64 = 0
  var _r1: U64 = 0

  new create() =>
    """
    Initialises with no rights.
    """
    clear()

  new from(caps: FileCaps box) =>
    """
    Initialises with the rights from a FileCaps.
    """
    clear()

    if caps(FileCreate) then set(Cap.creat()) end
    if caps(FileChmod) then set(Cap.fchmod()) end
    if caps(FileChown) then set(Cap.fchown()) end

    if caps(FileLink) then
      set(Cap.linkat())
      set(Cap.symlinkat())
    end

    if caps(FileLookup) then set(Cap.lookup()) end
    if caps(FileMkdir) then set(Cap.mkdirat()) end
    if caps(FileRead) then set(Cap.read()) end
    if caps(FileRemove) then set(Cap.unlinkat()) end
    if caps(FileRename) then set(Cap.renameat()) end
    if caps(FileSeek) then set(Cap.seek()) end

    if caps(FileStat) then
      set(Cap.fstat())
      set(Cap.fstatfs())
      set(Cap.fcntl())
    end

    if caps(FileSync) then set(Cap.fsync()) end
    if caps(FileTime) then set(Cap.futimes()) end
    if caps(FileTruncate) then set(Cap.ftruncate()) end
    if caps(FileWrite) then set(Cap.write()) end
    if caps(FileExec) then set(Cap.fexecve()) end

  new descriptor(fd: I32) =>
    """
    Initialises with the rights on the given file descriptor.
    """
    ifdef freebsd or "capsicum" then
      @__cap_rights_get(_version(), fd, addressof _r0)
    end

  fun ref set(cap: U64) =>
    ifdef freebsd or "capsicum" then
      @__cap_rights_set(addressof _r0, cap, U64(0))
    end

  fun ref unset(cap: U64) =>
    ifdef freebsd or "capsicum" then
      @__cap_rights_clear(addressof _r0, cap, U64(0))
    end

  fun limit(fd: I32): Bool =>
    """
    Limits the fd to the encoded rights.
    """
    ifdef freebsd or "capsicum" then
      @cap_rights_limit(fd, addressof _r0) == 0
    else
      true
    end

  fun ref merge(that: CapRights0) =>
    """
    Merge the rights in that into this.
    """
    ifdef freebsd or "capsicum" then
      @cap_rights_merge(addressof _r0, addressof that._r0)
    end

  fun ref remove(that: CapRights0) =>
    """
    Remove the rights in that from this.
    """
    ifdef freebsd or "capsicum" then
      @cap_rights_remove(addressof _r0, addressof that._r0)
    end

  fun ref clear() =>
    """
    Clear all rights.
    """
    ifdef freebsd or "capsicum" then
      @__cap_rights_init(_version(), addressof _r0, U64(0))
    end

  fun contains(that: CapRights0): Bool =>
    """
    Check that this is a superset of the rights in that.
    """
    ifdef freebsd or "capsicum" then
      @cap_rights_contains(addressof _r0, addressof that._r0)
    else
      true
    end

  fun _version(): I32 => 0
