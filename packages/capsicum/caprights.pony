use "files"

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
    if caps(FileLink) then set(Cap.linkat()).set(Cap.symlinkat()) end
    if caps(FileLookup) then set(Cap.lookup()) end
    if caps(FileMkdir) then set(Cap.mkdirat()) end
    if caps(FileRead) then set(Cap.read()) end
    if caps(FileRemove) then set(Cap.unlinkat()) end
    if caps(FileRename) then set(Cap.renameat()) end
    if caps(FileSeek) then set(Cap.seek()) end
    if caps(FileStat) then set(Cap.fstat()) end
    if caps(FileSync) then set(Cap.fsync()) end
    if caps(FileTime) then set(Cap.futimes()) end
    if caps(FileTruncate) then set(Cap.ftruncate()) end
    if caps(FileWrite) then set(Cap.write()) end

  new descriptor(fd: I32) =>
    """
    Initialises with the rights on the given file descriptor.
    """
    @__cap_rights_get[Pointer[U64]](I32(0), fd, &_r0)

  fun ref set(cap: U64): CapRights0^ =>
    @__cap_rights_set[Pointer[U64]](&_r0, cap, U64(0))
    this

  fun ref unset(cap: U64): CapRights0^ =>
    @__cap_rights_clear[Pointer[U64]](&_r0, cap, U64(0))
    this

  fun limit(fd: I32): Bool =>
    """
    Limits the fd to the encoded rights.
    """
    @cap_rights_limit[I32](fd, &_r0) == 0

  fun ref merge(that: CapRights0) =>
    """
    Merge the rights in that into this.
    """
    @cap_rights_merge[Pointer[U64]](&_r0, &that._r0)

  fun ref remove(that: CapRights0) =>
    """
    Remove the rights in that from this.
    """
    @cap_rights_remove[Pointer[U64]](&_r0, &that._r0)

  fun ref clear() =>
    """
    Clear all rights.
    """
    @__cap_rights_init[Pointer[U64]](I32(0), &_r0, U64(0))

  fun contains(that: CapRights0): Bool =>
    """
    Check that this is a superset of the rights in that.
    """
    @cap_rights_contains[Bool](&_r0, &that._r0)
