use @stringtab_init[None]()
use @stringtab_done[None]()

use @stringtab[Pointer[U8] ref](string: Pointer[U8] tag)
use @stringtab_len[Pointer[U8] ref](string: Pointer[U8] tag, len: USize)


primitive StringTab
  """
  The pony string table for interned strings during compilation.
  """
  fun _init() =>
    @stringtab_init()

  fun _final() =>
    @stringtab_done()

  fun apply(string: String box): String val =>
    """
    Stores the given string in the stringtab if it is not yet stored.
    And returns a reference to the stored string.
    This string will live as long as the primitive (the whole program lifetime
    usually).

    Might only fail if we can't alloc anything anymore.
    """
    let len = string.size()
    let str_ptr = string.cpointer()
    recover
      let ptr = @stringtab_len(str_ptr, len)
      String.from_cpointer(ptr, len) 
    end


