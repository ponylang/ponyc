class FileMode
  """
  This stores a UNIX-style mode broken out into a Bool for each bit. For other
  operating systems, the mapping will be approximate. For example, on Windows,
  if the file is readable all the read Bools will be set, and if the file is
  writeable, all the write Bools will be set.
  """
  var setuid: Bool = false
  var setgid: Bool = false
  var sticky: Bool = false
  var owner_read: Bool = false
  var owner_write: Bool = false
  var owner_exec: Bool = false
  var group_read: Bool = false
  var group_write: Bool = false
  var group_exec: Bool = false
  var any_read: Bool = false
  var any_write: Bool = false
  var any_exec: Bool = false
