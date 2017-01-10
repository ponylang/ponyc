class FileMode
  """
  This stores a UNIX-style mode broken out into a Bool for each bit. For other
  operating systems, the mapping will be approximate. For example, on Windows,
  if the file is readable all the read Bools will be set, and if the file is
  writeable, all the write Bools will be set.

  The default mode is read/write for the owner, read-only for everyone else.
  """
  var setuid: Bool = false
  var setgid: Bool = false
  var sticky: Bool = false
  var owner_read: Bool = true
  var owner_write: Bool = true
  var owner_exec: Bool = false
  var group_read: Bool = true
  var group_write: Bool = false
  var group_exec: Bool = false
  var any_read: Bool = true
  var any_write: Bool = false
  var any_exec: Bool = false

  fun ref exec() =>
    """
    Set the executable flag for everyone.
    """
    owner_exec = true
    group_exec = true
    any_exec = true

  fun ref shared() =>
    """
    Set the write flag for everyone to the same as owner_write.
    """
    group_write = owner_write
    any_write = owner_write

  fun ref group() =>
    """
    Clear all of the any-user flags.
    """
    any_read = false
    any_write = false
    any_exec = false

  fun ref private() =>
    """
    Clear all of the group and any-user flags.
    """
    group_read = false
    group_write = false
    group_exec = false
    any_read = false
    any_write = false
    any_exec = false

  fun _os(): U32 =>
    """
    Get the OS specific integer for a file mode. On Windows, if any read flag
    is set, the path is made readable, and if any write flag is set, the path
    is made writeable.
    """
    var m: U32 = 0

    ifdef windows then
      if owner_read or group_read or any_read then
        m = m or 0x100
      end

      if owner_write or group_write or any_write then
        m = m or 0x80
      end
    else
      if setuid then m = m or 0x800 end
      if setgid then m = m or 0x400 end
      if sticky then m = m or 0x200 end
      if owner_read then m = m or 0x100 end
      if owner_write then m = m or 0x80 end
      if owner_exec then m = m or 0x40 end
      if group_read then m = m or 0x20 end
      if group_write then m = m or 0x10 end
      if group_exec then m = m or 0x8 end
      if any_read then m = m or 0x4 end
      if any_write then m = m or 0x2 end
      if any_exec then m = m or 0x1 end
    end
    m
