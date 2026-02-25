use @userflags_create[Pointer[_UserFlags] val]()
use @userflags_free[None](userflags: Pointer[_UserFlags] box)
use @define_userflag[Bool](userflags: Pointer[_UserFlags] box, name: Pointer[U8] tag)
use @is_userflag_defined[Bool](userflags: Pointer[_UserFlags] box, name: Pointer[U8] tag)
use @remove_userflags[Bool](userflags: Pointer[_UserFlags] box, flags_to_remove: Pointer[Pointer[U8]] box)
use @clear_userflags[None](userflags: Pointer[_UserFlags] box)


primitive _UserFlags
  """
  A Stub implementation for userflags_t
  """
